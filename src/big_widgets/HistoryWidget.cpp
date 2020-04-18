#include "HistoryWidget.h"

#include <CommitHistoryModel.h>
#include <CommitHistoryView.h>
#include <RepositoryViewDelegate.h>
#include <BranchesWidget.h>
#include <WorkInProgressWidget.h>
#include <CommitInfoWidget.h>
#include <CommitInfo.h>
#include <GitQlientSettings.h>
#include <GitBase.h>
#include <GitBranches.h>
#include <GitRepoLoader.h>
#include <GitRemote.h>
#include <GitMerge.h>

#include <QLogger.h>

#include <QGridLayout>
#include <QLineEdit>
#include <QStackedWidget>
#include <QCheckBox>
#include <QMessageBox>
#include <QApplication>

using namespace QLogger;

HistoryWidget::HistoryWidget(const QSharedPointer<RevisionsCache> &cache, const QSharedPointer<GitBase> git,
                             QWidget *parent)
   : QFrame(parent)
   , mGit(git)
   , mCache(cache)
   , mRepositoryModel(new CommitHistoryModel(mCache, git))
   , mRepositoryView(new CommitHistoryView(mCache, git))
   , mBranchesWidget(new BranchesWidget(git))
   , mSearchInput(new QLineEdit())
   , mCommitStackedWidget(new QStackedWidget())
   , mCommitWidget(new WorkInProgressWidget(mCache, git))
   , mRevisionWidget(new CommitInfoWidget(mCache, git))
   , mChShowAllBranches(new QCheckBox(tr("Show all branches")))
{
   setAttribute(Qt::WA_DeleteOnClose);

   mCommitStackedWidget->setCurrentIndex(0);
   mCommitStackedWidget->addWidget(mRevisionWidget);
   mCommitStackedWidget->addWidget(mCommitWidget);
   mCommitStackedWidget->setFixedWidth(310);

   connect(mCommitWidget, &WorkInProgressWidget::signalShowDiff, this, &HistoryWidget::signalShowDiff);
   connect(mCommitWidget, &WorkInProgressWidget::signalChangesCommitted, this, &HistoryWidget::signalChangesCommitted);
   connect(mCommitWidget, &WorkInProgressWidget::signalCheckoutPerformed, this, &HistoryWidget::signalUpdateUi);
   connect(mCommitWidget, &WorkInProgressWidget::signalShowFileHistory, this, &HistoryWidget::signalShowFileHistory);
   connect(mCommitWidget, &WorkInProgressWidget::signalUpdateWip, this, &HistoryWidget::signalUpdateWip);

   connect(mRevisionWidget, &CommitInfoWidget::signalOpenFileCommit, this, &HistoryWidget::signalShowDiff);
   connect(mRevisionWidget, &CommitInfoWidget::signalShowFileHistory, this, &HistoryWidget::signalShowFileHistory);

   mSearchInput->setPlaceholderText(tr("Press Enter to search by SHA or log message..."));
   connect(mSearchInput, &QLineEdit::returnPressed, this, &HistoryWidget::search);

   connect(mRepositoryView, &CommitHistoryView::signalViewUpdated, this, &HistoryWidget::signalViewUpdated);
   connect(mRepositoryView, &CommitHistoryView::signalOpenDiff, this, &HistoryWidget::signalOpenDiff);
   connect(mRepositoryView, &CommitHistoryView::signalOpenCompareDiff, this, &HistoryWidget::signalOpenCompareDiff);
   connect(mRepositoryView, &CommitHistoryView::clicked, this, &HistoryWidget::commitSelected);
   connect(mRepositoryView, &CommitHistoryView::customContextMenuRequested, this, [this](const QPoint &pos) {
      const auto rowIndex = mRepositoryView->indexAt(pos);
      commitSelected(rowIndex);
   });
   connect(mRepositoryView, &CommitHistoryView::doubleClicked, this, &HistoryWidget::openDiff);
   connect(mRepositoryView, &CommitHistoryView::signalAmendCommit, this, &HistoryWidget::onAmendCommit);
   connect(mRepositoryView, &CommitHistoryView::signalMergeRequired, this, &HistoryWidget::mergeBranch);
   connect(mRepositoryView, &CommitHistoryView::signalCherryPickConflict, this,
           &HistoryWidget::signalCherryPickConflict);
   connect(mRepositoryView, &CommitHistoryView::signalPullConflict, this, &HistoryWidget::signalPullConflict);

   mRepositoryView->setModel(mRepositoryModel);
   mRepositoryView->setItemDelegate(mItemDelegate = new RepositoryViewDelegate(cache, git, mRepositoryView));
   mRepositoryView->setEnabled(true);

   connect(mBranchesWidget, &BranchesWidget::signalBranchesUpdated, this, &HistoryWidget::signalUpdateCache);
   connect(mBranchesWidget, &BranchesWidget::signalBranchCheckedOut, this, &HistoryWidget::onBranchCheckout);

   connect(mBranchesWidget, &BranchesWidget::signalSelectCommit, mRepositoryView, &CommitHistoryView::focusOnCommit);
   connect(mBranchesWidget, &BranchesWidget::signalSelectCommit, this,
           qOverload<const QString &>(&HistoryWidget::goToSha));
   connect(mBranchesWidget, &BranchesWidget::signalOpenSubmodule, this, &HistoryWidget::signalOpenSubmodule);
   connect(mBranchesWidget, &BranchesWidget::signalMergeRequired, this, &HistoryWidget::mergeBranch);
   connect(mBranchesWidget, &BranchesWidget::signalPullConflict, this, &HistoryWidget::signalPullConflict);

   GitQlientSettings settings;

   mChShowAllBranches->setChecked(settings.value("ShowAllBranches", true).toBool());
   connect(mChShowAllBranches, &QCheckBox::toggled, this, &HistoryWidget::onShowAllUpdated);

   const auto graphOptionsLayout = new QHBoxLayout();
   graphOptionsLayout->setContentsMargins(QMargins());
   graphOptionsLayout->setSpacing(10);
   graphOptionsLayout->addWidget(mSearchInput);
   graphOptionsLayout->addWidget(mChShowAllBranches);

   const auto viewLayout = new QVBoxLayout();
   viewLayout->setContentsMargins(QMargins());
   viewLayout->setSpacing(5);
   viewLayout->addLayout(graphOptionsLayout);
   viewLayout->addWidget(mRepositoryView);

   const auto layout = new QHBoxLayout();
   layout->setContentsMargins(QMargins());
   layout->setSpacing(15);
   layout->addWidget(mCommitStackedWidget);
   layout->addLayout(viewLayout);
   layout->addWidget(mBranchesWidget);

   setLayout(layout);
}

HistoryWidget::~HistoryWidget()
{
   delete mItemDelegate;
   delete mRepositoryModel;
}

void HistoryWidget::clear()
{
   mRepositoryView->clear();
   resetWip();
   mBranchesWidget->clear();
   mRevisionWidget->clear();

   mCommitStackedWidget->setCurrentIndex(mCommitStackedWidget->currentIndex());
}

void HistoryWidget::resetWip()
{
   mCommitWidget->clear();
}

void HistoryWidget::reload()
{
   mBranchesWidget->showBranches();

   const auto commitStackedIndex = mCommitStackedWidget->currentIndex();
   const auto currentSha = commitStackedIndex == 0 ? mRevisionWidget->getCurrentCommitSha() : CommitInfo::ZERO_SHA;

   focusOnCommit(currentSha);

   if (commitStackedIndex == 1)
      mCommitWidget->configure(currentSha);
}

void HistoryWidget::updateUiFromWatcher()
{
   const auto commitStackedIndex = mCommitStackedWidget->currentIndex();

   if (commitStackedIndex == 1 && !mCommitWidget->isAmendActive())
      mCommitWidget->configure(CommitInfo::ZERO_SHA);
}

void HistoryWidget::focusOnCommit(const QString &sha)
{
   mRepositoryView->focusOnCommit(sha);
}

QString HistoryWidget::getCurrentSha() const
{
   return mRepositoryView->getCurrentSha();
}

void HistoryWidget::onNewRevisions(int totalCommits)
{
   mRepositoryModel->onNewRevisions(totalCommits);

   onCommitSelected(CommitInfo::ZERO_SHA);

   mRepositoryView->selectionModel()->select(
       QItemSelection(mRepositoryModel->index(0, 0), mRepositoryModel->index(0, mRepositoryModel->columnCount() - 1)),
       QItemSelectionModel::Select);
}

void HistoryWidget::search()
{
   const auto text = mSearchInput->text();

   if (!text.isEmpty())
   {
      auto commitInfo = mCache->getCommitInfo(text);

      if (commitInfo.isValid())
         goToSha(text);
      else
      {
         auto selectedItems = mRepositoryView->selectedIndexes();
         auto startingRow = 0;

         if (!selectedItems.isEmpty())
         {
            std::sort(selectedItems.begin(), selectedItems.end(),
                      [](const QModelIndex index1, const QModelIndex index2) { return index1.row() <= index2.row(); });
            startingRow = selectedItems.constFirst().row();
         }

         commitInfo = mCache->getCommitInfoByField(CommitInfo::Field::SHORT_LOG, text, startingRow + 1);

         if (commitInfo.isValid())
            goToSha(commitInfo.sha());
      }
   }
}

void HistoryWidget::goToSha(const QString &sha)
{
   mRepositoryView->focusOnCommit(sha);

   onCommitSelected(sha);
}

void HistoryWidget::commitSelected(const QModelIndex &index)
{
   const auto sha = mRepositoryModel->sha(index.row());

   onCommitSelected(sha);
}

void HistoryWidget::openDiff(const QModelIndex &index)
{
   const auto sha = mRepositoryModel->sha(index.row());

   emit signalOpenDiff(sha);
}

void HistoryWidget::onShowAllUpdated(bool showAll)
{
   GitQlientSettings settings;
   settings.setValue("ShowAllBranches", showAll);

   emit signalAllBranchesActive(showAll);
}

void HistoryWidget::onBranchCheckout()
{
   QScopedPointer<GitBranches> gitBranches(new GitBranches(mGit));
   const auto ret = gitBranches->getLastCommitOfBranch(mGit->getCurrentBranch());

   if (mChShowAllBranches->isChecked())
      mRepositoryView->focusOnCommit(ret.output.toString());

   emit signalUpdateCache();
}

void HistoryWidget::mergeBranch(const QString &current, const QString &branchToMerge)
{
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
   QScopedPointer<GitMerge> git(new GitMerge(mGit, mCache));
   const auto ret = git->merge(current, { branchToMerge });

   QScopedPointer<GitRepoLoader> gitLoader(new GitRepoLoader(mGit, mCache));
   gitLoader->updateWipRevision();

   QApplication::restoreOverrideCursor();

   if (ret.output.toString().contains("merge failed", Qt::CaseInsensitive))
   {
      QMessageBox::critical(parentWidget(), tr("Merge failed"), ret.output.toString());

      emit signalMergeConflicts();
   }
   else
   {
      const auto outputStr = ret.output.toString();

      if (!outputStr.isEmpty())
      {
         if (ret.success)
         {
            emit signalUpdateCache();
            QMessageBox::information(parentWidget(), tr("Merge status"), outputStr);
         }
         else
            QMessageBox::warning(parentWidget(), tr("Merge status"), outputStr);
      }
   }
}

void HistoryWidget::onCommitSelected(const QString &goToSha)
{
   const auto isWip = goToSha == CommitInfo::ZERO_SHA;
   mCommitStackedWidget->setCurrentIndex(isWip);

   QLog_Info("UI", QString("Selected commit {%1}").arg(goToSha));

   if (isWip)
      mCommitWidget->configure(goToSha);
   else
      mRevisionWidget->configure(goToSha);
}

void HistoryWidget::onAmendCommit(const QString &sha)
{
   mCommitStackedWidget->setCurrentIndex(1);
   mCommitWidget->configure(sha);
}

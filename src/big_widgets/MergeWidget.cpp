#include "MergeWidget.h"

#include <GitQlientStyles.h>
#include <GitBase.h>
#include <GitMerge.h>
#include <GitRemote.h>
#include <GitLocal.h>
#include <FileDiffWidget.h>
#include <CommitInfo.h>
#include <RevisionFiles.h>
#include <FileEditor.h>
#include <GitQlientSettings.h>
#include <QPinnableTabWidget.h>

#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QMessageBox>
#include <QStackedWidget>

MergeWidget::MergeWidget(const QSharedPointer<GitCache> &gitQlientCache, const QSharedPointer<GitBase> &git,
                         QWidget *parent)
   : QFrame(parent)
   , mGitQlientCache(gitQlientCache)
   , mGit(git)
   , mConflictFiles(new QListWidget())
   , mMergedFiles(new QListWidget())
   , mCommitTitle(new QLineEdit())
   , mDescription(new QTextEdit())
   , mMergeBtn(new QPushButton(tr("Merge && Commit")))
   , mAbortBtn(new QPushButton(tr("Abort merge")))
   , mStacked(new QStackedWidget())
   , mFileDiff(new FileDiffWidget(mGit, mGitQlientCache))
{
   const auto autoMergedBtnFrame = new QFrame();
   autoMergedBtnFrame->setObjectName("DiffButtonsFrame");

   mCommitTitle->setObjectName("leCommitTitle");

   mDescription->setMaximumHeight(125);
   mDescription->setPlaceholderText(tr("Description"));
   mDescription->setObjectName("teDescription");
   mDescription->setLineWrapMode(QTextEdit::WidgetWidth);
   mDescription->setReadOnly(false);
   mDescription->setAcceptRichText(false);

   mAbortBtn->setObjectName("warningButton");
   mMergeBtn->setObjectName("applyActionBtn");

   const auto mergeBtnLayout = new QHBoxLayout();
   mergeBtnLayout->setContentsMargins(QMargins());
   mergeBtnLayout->addWidget(mAbortBtn);
   mergeBtnLayout->addStretch();
   mergeBtnLayout->addWidget(mMergeBtn);

   const auto mergeInfoLayout = new QVBoxLayout();
   mergeInfoLayout->setContentsMargins(QMargins());
   mergeInfoLayout->setSpacing(0);
   mergeInfoLayout->addWidget(mCommitTitle);
   mergeInfoLayout->addWidget(mDescription);
   mergeInfoLayout->addSpacerItem(new QSpacerItem(1, 10, QSizePolicy::Fixed, QSizePolicy::Fixed));
   mergeInfoLayout->addLayout(mergeBtnLayout);

   const auto mergeFrame = new QFrame();
   mergeFrame->setObjectName("mergeFrame");

   const auto conflictsLabel = new QLabel(tr("Conflicts"));
   conflictsLabel->setObjectName("FilesListTitle");

   const auto automergeLabel = new QLabel(tr("Changes to be committed"));
   automergeLabel->setObjectName("FilesListTitle");

   const auto mergeLayout = new QVBoxLayout(mergeFrame);
   mergeLayout->setContentsMargins(QMargins());
   mergeLayout->setSpacing(0);
   mergeLayout->addWidget(conflictsLabel);
   mergeLayout->addWidget(mConflictFiles);
   mergeLayout->addStretch(1);
   mergeLayout->addWidget(automergeLabel);
   mergeLayout->addWidget(mMergedFiles);
   mergeLayout->addStretch(2);
   mergeLayout->addLayout(mergeInfoLayout);

   mFileDiff->hideBackButton();

   const auto noFileFrame = new QFrame();
   const auto noFileLayout = new QGridLayout();
   noFileLayout->setContentsMargins(0, 0, 0, 0);
   noFileLayout->setSpacing(0);
   noFileLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding), 0, 0);
   noFileLayout->addWidget(new QLabel(tr("Select a file from the list to show its contents.")), 1, 1);
   noFileLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding), 2, 2);
   noFileFrame->setLayout(noFileLayout);

   mStacked->insertWidget(0, noFileFrame);
   mStacked->insertWidget(1, mFileDiff);

   const auto layout = new QHBoxLayout(this);
   layout->setContentsMargins(QMargins());
   layout->addWidget(mergeFrame);
   layout->addWidget(mStacked);

   connect(mFileDiff, &FileDiffWidget::exitRequested, this, [this]() { mStacked->setCurrentIndex(0); });
   connect(mFileDiff, &FileDiffWidget::fileStaged, this, &MergeWidget::onConflictResolved);

   connect(mConflictFiles, &QListWidget::itemClicked, this, &MergeWidget::changeDiffView);
   connect(mConflictFiles, &QListWidget::itemDoubleClicked, this, &MergeWidget::changeDiffView);
   connect(mMergedFiles, &QListWidget::itemClicked, this, &MergeWidget::changeDiffView);
   connect(mMergedFiles, &QListWidget::itemDoubleClicked, this, &MergeWidget::changeDiffView);
   connect(mAbortBtn, &QPushButton::clicked, this, &MergeWidget::abort);
   connect(mMergeBtn, &QPushButton::clicked, this, &MergeWidget::commit);
}

void MergeWidget::configure(const RevisionFiles &files, ConflictReason reason)
{
   mReason = reason;

   mConflictFiles->clear();
   mMergedFiles->clear();
   mFileDiff->clear();

   QFile mergeMsg(QString(mGit->getGitQlientSettingsDir() + QString::fromUtf8("/MERGE_MSG")));

   if (mergeMsg.open(QIODevice::ReadOnly))
   {
      const auto summary = QString::fromUtf8(mergeMsg.readLine()).trimmed();
      const auto description = QString::fromUtf8(mergeMsg.readAll()).trimmed();
      mCommitTitle->setText(summary);
      mDescription->setText(description);
      mergeMsg.close();
   }

   fillButtonFileList(files);
}

void MergeWidget::fillButtonFileList(const RevisionFiles &files)
{
   for (auto i = 0; i < files.count(); ++i)
   {
      const auto fileName = files.getFile(i);
      const auto fileInConflict = files.statusCmp(i, RevisionFiles::CONFLICT);
      const auto item = new QListWidgetItem(fileName);
      item->setData(Qt::UserRole, fileInConflict);

      fileInConflict ? mConflictFiles->addItem(item) : mMergedFiles->addItem(item);
   }
}

void MergeWidget::changeDiffView(QListWidgetItem *item)
{
   const auto file = item->text();
   const auto wip = mGitQlientCache->getCommitInfo(CommitInfo::ZERO_SHA);

   const auto configured
       = mFileDiff->configure(CommitInfo::ZERO_SHA, wip.parent(0), mGit->getWorkingDir() + "/" + file, false);

   mStacked->setCurrentIndex(configured);

   if (!configured)
      QMessageBox::warning(this, tr("No diff to show"), tr("There is not diff information to be shown."));
}

void MergeWidget::abort()
{
   GitExecResult ret;

   switch (mReason)
   {
      case ConflictReason::Pull:
      case ConflictReason::Merge: {
         QScopedPointer<GitMerge> git(new GitMerge(mGit, mGitQlientCache));
         ret = git->abortMerge();
         break;
      }
      case ConflictReason::CherryPick: {
         QScopedPointer<GitLocal> git(new GitLocal(mGit));
         ret = git->cherryPickAbort();
         break;
      }
      default:
         break;
   }

   if (!ret.success)
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error aborting"),
                         tr("There were problems during the aborting the merge. Please, see the detailed "
                            "description for more information."),
                         QMessageBox::Ok, this);
      msgBox.setDetailedText(ret.output.toString());
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
   else
   {
      removeMergeComponents();

      emit signalMergeFinished();
   }
}

void MergeWidget::commit()
{
   GitExecResult ret;

   switch (mReason)
   {
      case ConflictReason::Pull:
      case ConflictReason::Merge: {
         QScopedPointer<GitMerge> git(new GitMerge(mGit, mGitQlientCache));
         ret = git->applyMerge();
         break;
      }
      case ConflictReason::CherryPick: {
         QScopedPointer<GitLocal> git(new GitLocal(mGit));
         ret = git->cherryPickContinue();
         break;
      }
      default:
         break;
   }

   if (!ret.success)
   {
      QMessageBox msgBox(QMessageBox::Critical, tr("Error while merging"),
                         tr("There were problems during the merge operation. Please, see the detailed description "
                            "for more information."),
                         QMessageBox::Ok, this);
      msgBox.setDetailedText(ret.output.toString());
      msgBox.setStyleSheet(GitQlientStyles::getStyles());
      msgBox.exec();
   }
   else
   {
      removeMergeComponents();

      emit signalMergeFinished();
   }
}

void MergeWidget::removeMergeComponents()
{
   mCommitTitle->clear();
   mDescription->clear();

   mConflictFiles->clear();
   mMergedFiles->clear();
   mFileDiff->clear();
}

void MergeWidget::onConflictResolved(const QString &)
{
   const auto currentRow = mConflictFiles->currentRow();
   const auto currentConflict = mConflictFiles->takeItem(currentRow);

   if (currentConflict)
   {
      const auto fileName = currentConflict->text();
      delete currentConflict;

      mMergedFiles->addItem(fileName);
   }

   mConflictFiles->clearSelection();
   mConflictFiles->selectionModel()->clearSelection();
   mConflictFiles->selectionModel()->clearCurrentIndex();

   mFileDiff->clear();
   mStacked->setCurrentIndex(0);
}

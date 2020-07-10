#include <CommitInfoWidget.h>

#include <RevisionsCache.h>
#include <CommitInfo.h>
#include <FileListWidget.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QDateTime>
#include <QScrollArea>

#include <QLogger.h>

using namespace QLogger;

CommitInfoWidget::CommitInfoWidget(const QSharedPointer<RevisionsCache> &cache, const QSharedPointer<GitBase> &git,
                                   QWidget *parent)
   : QWidget(parent)
   , mCache(cache)
   , mGit(git)
   , labelSha(new QLabel())
   , labelTitle(new QLabel())
   , labelDescription(new QLabel())
   , labelAuthor(new QLabel())
   , labelDateTime(new QLabel())
   , labelEmail(new QLabel())
   , fileListWidget(new FileListWidget(mGit, mCache))
   , labelModCount(new QLabel())
{
   setAttribute(Qt::WA_DeleteOnClose);

   labelSha->setObjectName("labelSha");
   labelSha->setAlignment(Qt::AlignCenter);
   labelSha->setWordWrap(true);

   QFont font1;
   font1.setBold(true);
   font1.setWeight(75);
   labelTitle->setFont(font1);
   labelTitle->setAlignment(Qt::AlignCenter);
   labelTitle->setWordWrap(true);
   labelTitle->setObjectName("labelTitle");

   labelDescription->setWordWrap(true);
   labelDescription->setObjectName("labelDescription");

   const auto commitInfoFrame = new QFrame();
   commitInfoFrame->setObjectName("commitInfoFrame");

   const auto verticalLayout_2 = new QVBoxLayout(commitInfoFrame);
   verticalLayout_2->setSpacing(15);
   verticalLayout_2->setContentsMargins(0, 0, 0, 0);
   verticalLayout_2->addWidget(labelAuthor);
   verticalLayout_2->addWidget(labelDateTime);
   verticalLayout_2->addWidget(labelEmail);

   const auto labelIcon = new QLabel();
   labelIcon->setScaledContents(false);
   labelIcon->setPixmap(QIcon(":/icons/file").pixmap(15, 15));

   fileListWidget->setObjectName("fileListWidget");

   const auto headerLayout = new QHBoxLayout();
   headerLayout->setContentsMargins(5, 0, 0, 0);
   headerLayout->setSpacing(0);
   headerLayout->addWidget(labelIcon);
   headerLayout->addSpacerItem(new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
   headerLayout->addWidget(new QLabel(tr("Files ")));
   headerLayout->addWidget(labelModCount);
   headerLayout->addStretch();

   const auto verticalLayout = new QVBoxLayout(this);
   verticalLayout->setSpacing(10);
   verticalLayout->setContentsMargins(0, 0, 0, 0);
   verticalLayout->addWidget(labelSha);
   verticalLayout->addWidget(labelTitle);
   verticalLayout->addWidget(labelDescription);
   verticalLayout->addWidget(commitInfoFrame);
   verticalLayout->addLayout(headerLayout);
   verticalLayout->addWidget(fileListWidget);

   connect(fileListWidget, &FileListWidget::itemDoubleClicked, this,
           [this](QListWidgetItem *item) { emit signalOpenFileCommit(mCurrentSha, mParentSha, item->text()); });
   connect(fileListWidget, &FileListWidget::signalShowFileHistory, this, &CommitInfoWidget::signalShowFileHistory);
   connect(fileListWidget, &FileListWidget::signalEditFile, this, &CommitInfoWidget::signalEditFile);
}

void CommitInfoWidget::configure(const QString &sha)
{
   if (sha == mCurrentSha)
      return;

   clear();

   mCurrentSha = sha;
   mParentSha = sha;

   if (sha != CommitInfo::ZERO_SHA && !sha.isEmpty())
   {
      const auto currentRev = mCache->getCommitInfo(sha);

      if (!currentRev.sha().isEmpty())
      {
         QLog_Info("UI", QString("Loading information of the commit {%1}").arg(sha));
         mCurrentSha = currentRev.sha();
         mParentSha = currentRev.parent(0);

         QDateTime commitDate = QDateTime::fromSecsSinceEpoch(currentRev.authorDate().toInt());
         labelSha->setText(sha);

         const auto author = currentRev.committer();
         const auto authorName = author.split("<").first();
         const auto email = author.split("<").last().split(">").first();

         labelEmail->setText(email);
         labelTitle->setText(currentRev.shortLog());
         labelAuthor->setText(authorName);
         labelDateTime->setText(commitDate.toString("dd/MM/yyyy hh:mm"));

         const auto description = currentRev.longLog();
         labelDescription->setText(description.isEmpty() ? "No description provided." : description);

         auto f = labelDescription->font();
         f.setItalic(description.isEmpty());
         labelDescription->setFont(f);

         fileListWidget->insertFiles(mCurrentSha, mParentSha);
         labelModCount->setText(QString("(%1)").arg(fileListWidget->count()));
      }
   }
}

QString CommitInfoWidget::getCurrentCommitSha() const
{
   return mCurrentSha;
}

void CommitInfoWidget::clear()
{
   mCurrentSha = QString();
   mParentSha = QString();

   fileListWidget->clear();
   labelSha->clear();
   labelEmail->clear();
   labelTitle->clear();
   labelAuthor->clear();
   labelDateTime->clear();
   labelDescription->clear();
}

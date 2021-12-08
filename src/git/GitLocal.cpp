#include "GitLocal.h"

#include <GitBase.h>
#include <GitWip.h>
#include <QLogger.h>
#include <RevisionFiles.h>

#include <QFile>
#include <QProcess>

using namespace QLogger;

namespace
{
static QString quote(const QStringList &sl)
{
   QString q(sl.join(QString("$%1$").arg(' ')));
   q.prepend("$").append("$");
   return q;
}
}

GitLocal::GitLocal(const QSharedPointer<GitBase> &gitBase)
   : mGitBase(gitBase)
{
}

GitExecResult GitLocal::stageFile(const QString &fileName) const
{
   QLog_Debug("Git", QString("Staging file: {%1}").arg(fileName));
   return mGitBase->run("git", {"add", fileName});
}

GitExecResult GitLocal::removeFile(const QString &fileName) const
{
   QLog_Debug("Git", QString("Removing file: {%1}").arg(fileName));
   return mGitBase->run("git", {"rm", fileName});
}

bool GitLocal::isInCherryPickMerge() const
{
   QFile cherrypickHead(QString("%1/CHERRY_PICK_HEAD").arg(mGitBase->getGitDir()));

   return cherrypickHead.exists();
}

GitExecResult GitLocal::cherryPickCommit(const QString &sha) const
{
   QLog_Debug("Git", QString("Cherry-picking commit: {%1}").arg(sha));
   return mGitBase->run("git", {"cherry-pick", sha});
}

GitExecResult GitLocal::cherryPickAbort() const
{
   QLog_Debug("Git", QString("Aborting cherryPick"));
   return mGitBase->run("git", {"cherry-pick", "--abort"});
}

GitExecResult GitLocal::cherryPickContinue(const QString &msg) const
{
   QLog_Debug("Git", QString("Applying cherryPick"));

   QString cmd;

   if (msg.isEmpty())
	  return mGitBase->run("git", {"cherry-pick", "--continue"});

   return mGitBase->run("git", {"commit", "-m", msg});
}

GitExecResult GitLocal::checkoutCommit(const QString &sha) const
{
   QLog_Debug("Git", QString("Checking out a commit: {%1}").arg(sha));
   const auto ret = mGitBase->run("git", {"checkout", sha});

   if (ret.success)
      mGitBase->updateCurrentBranch();

   return ret;
}

GitExecResult GitLocal::markFilesAsResolved(const QStringList &files)
{
   QLog_Debug("Git", QString("Marking {%1} files as resolved").arg(files.count()));
   return mGitBase->run("git", {"add", files.join(' ')});
}

bool GitLocal::checkoutFile(const QString &fileName) const
{
   if (fileName.isEmpty())
   {
      QLog_Warning("Git", QString("Executing checkoutFile with an empty file.").arg(fileName));

      return false;
   }

   QLog_Debug("Git", QString("Checking out a file: {%1}").arg(fileName));
   return mGitBase->run("git", {"checkout", fileName}).success;
}

GitExecResult GitLocal::resetFile(const QString &fileName) const
{
   QLog_Debug("Git", QString("Resetting file: {%1}").arg(fileName));
   return mGitBase->run("git", {"reset", fileName});
}

bool GitLocal::resetCommit(const QString &sha, CommitResetType type)
{
   QString typeStr;

   switch (type)
   {
      case CommitResetType::SOFT:
         typeStr = "soft";
         break;
      case CommitResetType::MIXED:
         typeStr = "mixed";
         break;
      case CommitResetType::HARD:
         typeStr = "hard";
         break;
   }

   QLog_Debug("Git", QString("Resetting commit: {%1} type {%2}").arg(sha, typeStr));
   return mGitBase->run("git", {"reset", "--" + typeStr, sha}).success;
}

GitExecResult GitLocal::commit(const QString &msg) const
{
   QLog_Debug("Git", QString("Commit changes"));
   return mGitBase->run("git", {"commit", "-m", msg});
}

GitExecResult GitLocal::ammend(const QString &msg) const
{
   QLog_Debug("Git", QString("Amend message"));

   return mGitBase->run("git", {"commit", "--amend", "-m", msg});
}

GitExecResult GitLocal::commitFiles(QStringList &selFiles, const RevisionFiles &allCommitFiles,
                                    const QString &msg) const
{
   QStringList notSel;

   for (auto i = 0; i < allCommitFiles.count(); ++i)
   {
      if (const auto &fp = allCommitFiles.getFile(i);
          selFiles.indexOf(fp) == -1 && allCommitFiles.statusCmp(i, RevisionFiles::IN_INDEX))
      {
         notSel.append(fp);
      }
   }

   if (const auto updIdx = updateIndex(allCommitFiles, selFiles); !updIdx.success)
      return updIdx;

   QLog_Debug("Git", QString("Committing files"));
   auto ret = mGitBase->run("git", {"commit", "-m", msg});

   if (ret.output.startsWith("On branch"))
      ret.output = false;

   return ret;
}

GitExecResult GitLocal::ammendCommit(const QStringList &selFiles, const RevisionFiles &allCommitFiles,
                                     const QString &msg, const QString &author) const
{
   QStringList notSel;

   for (auto i = 0; i < allCommitFiles.count(); ++i)
   {
      const QString &fp = allCommitFiles.getFile(i);
      if (selFiles.indexOf(fp) == -1 && allCommitFiles.statusCmp(i, RevisionFiles::IN_INDEX)
          && !allCommitFiles.statusCmp(i, RevisionFiles::DELETED))
         notSel.append(fp);
   }

   QLog_Debug("Git", QString("Amending files"));

   QString cmtOptions;

   if (!author.isEmpty())
	   return mGitBase->run("git", {"commit", "--amend", "--author", author, "-m", msg});

   return mGitBase->run("git", {"commit", "--amend", "-m", msg});
}

GitExecResult GitLocal::updateIndex(const RevisionFiles &files, const QStringList &selFiles) const
{
   QStringList toRemove;

   for (const auto &file : selFiles)
   {
      const auto index = files.mFiles.indexOf(file);

      if (index != -1 && files.statusCmp(index, RevisionFiles::DELETED))
         toRemove << file;
   }

   if (!toRemove.isEmpty())
   {
	  QLog_Trace("Git", QString("Updating index for files: {%1}").arg(quote(toRemove)));

	  const auto ret = mGitBase->run("git", {"rm", "--cached", "--ignore-unmatch", "--", quote(toRemove)});

      if (!ret.success)
         return ret;
   }

   const auto ret = GitExecResult(true, "Indexes updated");

   return ret;
}

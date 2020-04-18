#include "RevisionsCache.h"

#include <QLogger.h>

using namespace QLogger;

RevisionsCache::RevisionsCache(QObject *parent)
   : QObject(parent)
{
}

RevisionsCache::~RevisionsCache()
{
   for (auto commit : mCommits)
      delete commit;

   mCommits.clear();
   mCommitsMap.clear();
}

void RevisionsCache::configure(int numElementsToStore)
{
   QLog_Debug("Git", QString("Configuring the cache for {%1} elements.").arg(numElementsToStore));

   if (mCommits.isEmpty())
   {
      // We reserve 1 extra slots for the ZERO_SHA (aka WIP commit)
      mCommits.resize(numElementsToStore + 1);
      mCommitsMap.reserve(numElementsToStore + 1);
   }

   mCacheLocked = false;
}

CommitInfo RevisionsCache::getCommitInfoByRow(int row) const
{
   const auto commit = row >= 0 && row < mCommits.count() ? mCommits.at(row) : nullptr;

   return commit ? *commit : CommitInfo();
}

CommitInfo RevisionsCache::getCommitInfoByField(CommitInfo::Field field, const QString &text, int startingPoint)
{
   auto commitIter = searchCommit(field, text, startingPoint);

   if (commitIter == mCommits.constEnd() && startingPoint > 0)
      commitIter = searchCommit(field, text);

   return commitIter != mCommits.constEnd() ? **commitIter : CommitInfo();
}

CommitInfo RevisionsCache::getCommitInfo(const QString &sha) const
{
   if (!sha.isEmpty())
   {
      const auto c = mCommitsMap.value(sha, nullptr);

      if (c == nullptr)
      {
         const auto shas = mCommitsMap.keys();
         const auto it = std::find_if(shas.cbegin(), shas.cend(),
                                      [sha](const QString &shaToCompare) { return shaToCompare.startsWith(sha); });

         if (it != shas.cend())
            return *mCommitsMap.value(*it);

         return CommitInfo();
      }

      return *c;
   }

   return CommitInfo();
}

RevisionFiles RevisionsCache::getRevisionFile(const QString &sha1, const QString &sha2) const
{
   return mRevisionFilesMap.value(qMakePair(sha1, sha2));
}

Reference RevisionsCache::getReference(const QString &sha) const
{
   return mReferencesMap.value(sha, Reference());
}

void RevisionsCache::insertCommitInfo(CommitInfo rev)
{
   if (mCacheLocked)
      QLog_Warning("Git", QString("The cache is currently locked."));
   else if (mCommitsMap.contains(rev.sha()))
      QLog_Info("Git", QString("The commit with SHA {%1} is already in the cache.").arg(rev.sha()));
   else
   {
      updateLanes(rev);

      const auto commit = new CommitInfo(rev);

      if (rev.orderIdx >= mCommits.count())
      {
         QLog_Debug("Git", QString("Adding commit with sha {%1}.").arg(commit->sha()));

         mCommits.insert(rev.orderIdx, commit);
      }
      else if (!(mCommits[rev.orderIdx] && *mCommits[rev.orderIdx] == *commit))
      {
         QLog_Trace("Git", QString("Overwriting commit with sha {%1}.").arg(commit->sha()));

         delete mCommits[rev.orderIdx];
         mCommits[rev.orderIdx] = commit;
      }

      mCommitsMap.insert(rev.sha(), commit);

      if (mCommitsMap.contains(rev.parent(0)))
         mCommitsMap.remove(rev.parent(0));
   }
}

bool RevisionsCache::insertRevisionFile(const QString &sha1, const QString &sha2, const RevisionFiles &file)
{
   const auto key = qMakePair(sha1, sha2);

   if (!sha1.isEmpty() && !sha2.isEmpty() && mRevisionFilesMap.value(key) != file)
   {
      QLog_Debug("Git", QString("Adding the revisions files between {%1} and {%2}.").arg(sha1, sha2));

      mRevisionFilesMap.insert(key, file);

      return true;
   }

   return false;
}

void RevisionsCache::insertReference(const QString &sha, Reference ref)
{
   QLog_Debug("Git", QString("Adding a new reference with SHA {%1}.").arg(sha));

   mReferencesMap[sha] = std::move(ref);
}

void RevisionsCache::updateWipCommit(const QString &parentSha, const QString &diffIndex, const QString &diffIndexCache)
{
   QLog_Debug("Git", QString("Updating the WIP commit. The actual parent has SHA {%1}.").arg(parentSha));

   const auto key = qMakePair(CommitInfo::ZERO_SHA, parentSha);
   const auto fakeRevFile = fakeWorkDirRevFile(diffIndex, diffIndexCache);

   insertRevisionFile(CommitInfo::ZERO_SHA, parentSha, fakeRevFile);

   if (!mCacheLocked)
   {
      const QString longLog;
      const auto author = QString("-");
      const auto log
         = fakeRevFile.count() == mUntrackedfiles.count() ? QString("No local changes") : QString("Local changes");
      CommitInfo c(CommitInfo::ZERO_SHA, { parentSha }, author, QDateTime::currentDateTime().toSecsSinceEpoch(), log,
                   longLog, 0);
      c.isDiffCache = true;

      updateLanes(c);

      if (mCommits[c.orderIdx])
         c.lanes = mCommits[c.orderIdx]->lanes;

      const auto sha = c.sha();
      const auto commit = new CommitInfo(std::move(c));

      delete mCommits[commit->orderIdx];
      mCommits[commit->orderIdx] = commit;

      mCommitsMap.insert(sha, commit);
   }
}

void RevisionsCache::removeReference(const QString &sha)
{
   mReferencesMap.remove(sha);
}

bool RevisionsCache::containsRevisionFile(const QString &sha1, const QString &sha2) const
{
   return mRevisionFilesMap.contains(qMakePair(sha1, sha2));
}

void RevisionsCache::updateLanes(CommitInfo &c)
{
   const auto sha = c.sha();

   QLog_Trace("Git", QString("Updating the lanes for SHA {%1}.").arg(sha));

   if (mLanes.isEmpty())
      mLanes.init(c.sha());

   bool isDiscontinuity;
   bool isFork = mLanes.isFork(sha, isDiscontinuity);
   bool isMerge = (c.parentsCount() > 1);
   bool isInitial = (c.parentsCount() == 0);

   if (isDiscontinuity)
      mLanes.changeActiveLane(sha); // uses previous isBoundary state

   mLanes.setBoundary(c.isBoundary()); // update must be here

   if (isFork)
      mLanes.setFork(sha);
   if (isMerge)
      mLanes.setMerge(c.parents());
   if (isInitial)
      mLanes.setInitial();

   mLanes.setLanes(c.lanes); // here lanes are snapshotted

   const auto nextSha = isInitial ? QString() : c.parent(0);

   mLanes.nextParent(nextSha);

   if (isMerge)
      mLanes.afterMerge();
   if (isFork)
      mLanes.afterFork();
   if (mLanes.isBranch())
      mLanes.afterBranch();
}

RevisionFiles RevisionsCache::parseDiffFormat(const QString &buf, FileNamesLoader &fl)
{
   RevisionFiles rf;
   auto parNum = 1;
   const auto lines = buf.split("\n", QString::SkipEmptyParts);

   for (auto line : lines)
   {
      if (line[0] == ':') // avoid sha's in merges output
      {
         if (line[1] == ':')
         { // it's a combined merge
            /* For combined merges rename/copy information is useless
             * because nor the original file name, nor similarity info
             * is given, just the status tracks that in the left/right
             * branch a renamed/copy occurred (as example status could
             * be RM or MR). For visualization purposes we could consider
             * the file as modified
             */
            if (fl.rf != &rf)
            {
               flushFileNames(fl);
               fl.rf = &rf;
            }
            appendFileName(line.section('\t', -1), fl);
            rf.setStatus("M");
            rf.mergeParent.append(parNum);
         }
         else
         {
            if (line.at(98) == '\t') // Faster parsing in normal case
            {
               if (fl.rf != &rf)
               {
                  flushFileNames(fl);
                  fl.rf = &rf;
               }
               appendFileName(line.mid(99), fl);
               rf.setStatus(line.at(97));
               rf.mergeParent.append(parNum);
            }
            else // It's a rename or a copy, we are not in fast path now!
               setExtStatus(rf, line.mid(97), parNum, fl);
         }
      }
      else
         ++parNum;
   }

   return rf;
}

void RevisionsCache::appendFileName(const QString &name, FileNamesLoader &fl)
{
   int idx = name.lastIndexOf('/') + 1;
   const QString &dr = name.left(idx);
   const QString &nm = name.mid(idx);

   auto it = mDirNames.indexOf(dr);
   if (it == -1)
   {
      int idx = mDirNames.count();
      mDirNames.append(dr);
      fl.rfDirs.append(idx);
   }
   else
      fl.rfDirs.append(it);

   it = mFileNames.indexOf(nm);
   if (it == -1)
   {
      int idx = mFileNames.count();
      mFileNames.append(nm);
      fl.rfNames.append(idx);
   }
   else
      fl.rfNames.append(it);

   fl.files.append(name);
}

void RevisionsCache::flushFileNames(FileNamesLoader &fl)
{
   if (!fl.rf)
      return;

   for (auto i = 0; i < fl.rfNames.count(); ++i)
   {
      const auto dirName = mDirNames.at(fl.rfDirs.at(i));
      const auto fileName = mFileNames.at(fl.rfNames.at(i));

      if (!fl.rf->mFiles.contains(dirName + fileName))
         fl.rf->mFiles.append(dirName + fileName);
   }

   fl.rfNames.clear();
   fl.rfDirs.clear();
   fl.rf = nullptr;
}

bool RevisionsCache::pendingLocalChanges() const
{
   const auto commit = mCommitsMap.value(CommitInfo::ZERO_SHA);
   const auto rf = getRevisionFile(CommitInfo::ZERO_SHA, commit->parent(0));
   return rf.count() == mUntrackedfiles.count();
}

uint RevisionsCache::checkRef(const QString &sha, uint mask) const
{
   const auto ref = getReference(sha);

   return ref.isValid() ? ref.type & mask : 0;
}

const QStringList RevisionsCache::getRefNames(const QString &sha, uint mask) const
{
   QStringList result;
   if (!checkRef(sha, mask))
      return result;

   const auto rf = getReference(sha);

   if (mask & TAG)
      result << rf.tags;

   if (mask & BRANCH)
      result << rf.branches;

   if (mask & RMT_BRANCH)
      result << rf.remoteBranches;

   if (mask & REF)
      result << rf.refs;

   if (mask == APPLIED || mask == UN_APPLIED)
      result << QStringList(rf.stgitPatch);

   return result;
}

void RevisionsCache::setExtStatus(RevisionFiles &rf, const QString &rowSt, int parNum, FileNamesLoader &fl)
{
   const QStringList sl(rowSt.split('\t', QString::SkipEmptyParts));
   if (sl.count() != 3)
      return;

   // we want store extra info with format "orig --> dest (Rxx%)"
   // but git give us something like "Rxx\t<orig>\t<dest>"
   QString type = sl[0];
   type.remove(0, 1);
   const QString &orig = sl[1];
   const QString &dest = sl[2];
   const QString extStatusInfo(orig + " --> " + dest + " (" + QString::number(type.toInt()) + "%)");

   /*
    NOTE: we set rf.extStatus size equal to position of latest
          copied/renamed file. So it can have size lower then
          rf.count() if after copied/renamed file there are
          others. Here we have no possibility to know final
          dimension of this RefFile. We are still in parsing.
 */

   // simulate new file
   if (fl.rf != &rf)
   {
      flushFileNames(fl);
      fl.rf = &rf;
   }
   appendFileName(dest, fl);
   rf.mergeParent.append(parNum);
   rf.setStatus(RevisionFiles::NEW);
   rf.appendExtStatus(extStatusInfo);

   // simulate deleted orig file only in case of rename
   if (type.at(0) == 'R')
   { // renamed file
      if (fl.rf != &rf)
      {
         flushFileNames(fl);
         fl.rf = &rf;
      }
      appendFileName(orig, fl);
      rf.mergeParent.append(parNum);
      rf.setStatus(RevisionFiles::DELETED);
      rf.appendExtStatus(extStatusInfo);
   }
   rf.setOnlyModified(false);
}

QVector<CommitInfo *>::const_iterator RevisionsCache::searchCommit(CommitInfo::Field field, const QString &text,
                                                                   const int startingPoint) const
{
   return std::find_if(mCommits.constBegin() + startingPoint, mCommits.constEnd(),
                       [field, text](CommitInfo *info) { return info->getFieldStr(field).contains(text); });
}

void RevisionsCache::clear()
{
   mCacheLocked = true;
   mDirNames.clear();
   mFileNames.clear();
   mRevisionFilesMap.clear();
   mReferencesMap.clear();
   mLanes.clear();
   mCommitsMap.clear();
}

int RevisionsCache::count() const
{
   return mCommits.count();
}

int RevisionsCache::countReferences() const
{
   return mReferencesMap.count();
}

RevisionFiles RevisionsCache::fakeWorkDirRevFile(const QString &diffIndex, const QString &diffIndexCache)
{
   FileNamesLoader fl;
   auto rf = parseDiffFormat(diffIndex, fl);
   fl.rf = &rf;
   rf.setOnlyModified(false);

   for (const auto &it : qAsConst(mUntrackedfiles))
   {
      if (fl.rf != &rf)
      {
         flushFileNames(fl);
         fl.rf = &rf;
      }

      appendFileName(it, fl);
      rf.setStatus(RevisionFiles::UNKNOWN);
      rf.mergeParent.append(1);
   }

   RevisionFiles cachedFiles = parseDiffFormat(diffIndexCache, fl);
   flushFileNames(fl);

   for (auto i = 0; i < rf.count(); i++)
   {
      if (cachedFiles.mFiles.indexOf(rf.getFile(i)) != -1)
      {
         if (cachedFiles.statusCmp(i, RevisionFiles::CONFLICT))
            rf.appendStatus(i, RevisionFiles::CONFLICT);

         rf.appendStatus(i, RevisionFiles::IN_INDEX);
      }
   }

   return rf;
}

RevisionFiles RevisionsCache::parseDiff(const QString &logDiff)
{
   FileNamesLoader fl;

   auto rf = parseDiffFormat(logDiff, fl);
   fl.rf = &rf;
   flushFileNames(fl);

   return rf;
}

void RevisionsCache::setUntrackedFilesList(const QVector<QString> &untrackedFiles)
{
   mUntrackedfiles = untrackedFiles;
}

#include "RepoConfigDlg.h"
#include "ui_RepoConfigDlg.h"

#include <GitConfig.h>
#include <GitQlientStyles.h>

#include <QLabel>
#include <QLineEdit>
#include <QStyle>

RepoConfigDlg::RepoConfigDlg(const QSharedPointer<GitBase> &git, QWidget *parent)
   : QDialog(parent)
   , ui(new Ui::RepoConfigDlg)
   , mGit(git)
{
   ui->setupUi(this);

   ui->tabWidget->setCurrentIndex(0);

   QScopedPointer<GitConfig> gitConfig(new GitConfig(mGit));
   const auto localConfig = gitConfig->getLocalConfig();

   if (localConfig.success)
   {
      const auto layout = new QGridLayout(ui->tab);
      layout->setContentsMargins(10, 10, 10, 10);
      layout->setSpacing(10);
      layout->addWidget(new QLabel("KEY"), 0, 0);
      layout->addWidget(new QLabel("VALUE"), 0, 1);

      const auto elements = localConfig.output.toString().split('\n', QString::SkipEmptyParts);
      addUserConfig(elements, layout);
   }

   const auto globalConfig = gitConfig->getGlobalConfig();

   if (globalConfig.success)
   {
      const auto layout = new QGridLayout(ui->tab_2);
      layout->setContentsMargins(10, 10, 10, 10);
      layout->setSpacing(10);
      layout->addWidget(new QLabel("KEY"), 0, 0);
      layout->addWidget(new QLabel("VALUE"), 0, 1);

      const auto elements = globalConfig.output.toString().split('\n', QString::SkipEmptyParts);
      addUserConfig(elements, layout);
   }

   ui->tab->setStyleSheet("background-color: #404142;");
   ui->tab_2->setStyleSheet("background-color: #404142;");

   style()->unpolish(this);
   setStyleSheet(GitQlientStyles::getStyles());
   style()->polish(this);
}

RepoConfigDlg::~RepoConfigDlg()
{
   delete ui;
}

void RepoConfigDlg::addUserConfig(const QStringList &elements, QGridLayout *layout)
{
   auto row = 1;
   const auto configKey = QString("user.");

   for (const auto &element : elements)
   {
      if (element.startsWith("user."))
      {
         const auto pair = element.split("=");
         layout->addWidget(new QLabel(pair.first()), row, 0);

         const auto lineEdit = new QLineEdit();
         lineEdit->setText(pair.last());
         connect(lineEdit, &QLineEdit::editingFinished, this, &RepoConfigDlg::setConfig);

         layout->addWidget(lineEdit, row++, 1);

         mLineeditKeyMap.insert(lineEdit, pair.first());
      }
   }

   layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding), row, 0);
}

void RepoConfigDlg::setConfig()
{
   const auto lineEdit = qobject_cast<QLineEdit *>(sender());
   QScopedPointer<GitConfig> gitConfig(new GitConfig(mGit));

   switch (ui->tabWidget->currentIndex())
   {
      case 0: // Local
         gitConfig->setLocalData(mLineeditKeyMap.value(lineEdit), lineEdit->text());
         break;
      case 1: // Global
         gitConfig->setGlobalData(mLineeditKeyMap.value(lineEdit), lineEdit->text());
         break;
      default:
         break;
   }
}

INCLUDEPATH += $$PWD

HEADERS += \
   $$PWD/Comment.h \
   $$PWD/Commit.h \
   $$PWD/ConfigData.h \
   $$PWD/GitHubRestApi.h \
   $$PWD/GitLabRestApi.h \
   $$PWD/IRestApi.h \
   $$PWD/Issue.h \
   $$PWD/Label.h \
   $$PWD/Milestone.h \
   $$PWD/Platform.h \
   $$PWD/PullRequest.h \
   $$PWD/User.h \
   $$PWD/entity.h \
   $$PWD/md4c-html.h \
   $$PWD/md4c.h

SOURCES += \
   $$PWD/GitHubRestApi.cpp \
   $$PWD/GitLabRestApi.cpp \
   $$PWD/IRestApi.cpp \
   $$PWD/entity.c \
   $$PWD/md4c-html.c \
   $$PWD/md4c.c

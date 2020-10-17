![GitQlient logo](https://github.com/francescmm/GitQlient/blob/master/src/resources/icons/GitQlientLogo96.png "GitQlient")

# GitQlient: Multi-platform Git client written with Qt
Linux & OSX: [![Build Status](https://travis-ci.org/francescmm/GitQlient.svg?branch=master)](https://travis-ci.org/francescmm/GitQlient)

RPM build: [![Copr build status](https://copr.fedorainfracloud.org/coprs/gitqlient/GitQlient/package/gitqlient/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/gitqlient/GitQlient/package/gitqlient/)

Qt 5.15: [![Build status for Qt 5.15](https://ci.appveyor.com/api/projects/status/ihw50uwdiim952c0/branch/master?svg=true)](https://ci.appveyor.com/project/francescmm/gitqlient/branch/master)

Qt 5.14: [![Build status for Qt 5.14](https://ci.appveyor.com/api/projects/status/niu2b6m5gehxnarg/branch/master?svg=true)](https://ci.appveyor.com/project/francescmm/gitqlient-qt514/branch/master)

Qt 5.13: [![Build status for Qt 5.13](https://ci.appveyor.com/api/projects/status/kxidc23f7mnqyjyi/branch/master?svg=true)](https://ci.appveyor.com/project/francescmm/gitqlient-qt513/branch/master)

Qt 5.12: [![Build status for Qt 5.12](https://ci.appveyor.com/api/projects/status/oxt8t90vssooy9rm/branch/master?svg=true)](https://ci.appveyor.com/project/francescmm/gitqlient-qt512/branch/master)

GitQlient, pronounced as git+client (/gɪtˈklaɪənt/) is a multi-platform Git
client originally forked from QGit. Nowadays it goes beyond of just a fork and
adds a lot of new functionality.

![GitQlient main screen](/docs/assets/GitQlient.png)

Some of the major feature you can find are:

1. New features:
    1. Easy access to remote actions like: push, pull, submodules management and branches
    2. Branches management
    3. Tags and stashes management
    4. Submodules handling
    5. Allow to open several repositories in the same window
    6. Better visualization of the commits and the work in progress
    7. Better visualization of the repository view
    8. GitHub/GitLab integration
    9. Embedded text editor with syntax highlight for C++
2. Improved UI experience
    1. Easy access to the main Git actions
    2. Better code separation between Views and Models
    3. Simplification of the different options one can do, keeping it to what a Git client is

## User Manual

Please, if you have any doubts about how to use it or you just want to know all you can do with GitQlient, take a look to [the user manual in here](https://francescmm.github.io/GitQlient).

It is planned to release for Linux, MacOs and Windows. However, take into account that the development environment is based on Linux and it will be the first platform released.

## Development documentation

I'm aware that developers may like to have some more information beyond the User Manual. Whether you want to collaborate in the development or just to know how GitQlient works I think it's nice to have some development documentation. In the [Wiki section](https://github.com/francescmm/GitQlient/wiki) I will release class diagramas, sequence diagrams as well as the Release Plan an features. Take a look!

## Licenses

Most of the icons on GitQlient are from Font Awesome. [The license states is GPL friendly](https://fontawesome.com/license/free). Those icons that are not from Font Awesome are custom made icons.

The font used bt GitQlient is DejaVu Sans and DejaVu Sans Mono. It is a free font used by most of the Linux distros and [its license can be found on GitHub](https://github.com/dejavu-fonts/dejavu-fonts/blob/master/LICENSE).

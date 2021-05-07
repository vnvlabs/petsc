.. _doc_download:

=================
Downloading PETSc
=================

Recommended Download
====================

.. code-block:: console

   > git clone -b release https://gitlab.com/petsc/petsc.git petsc

Use ``git pull`` to download any new patches or changes that have been added since your
``git clone`` or last ``git pull``. Use ``git checkout vMAJOR.MINOR.PATCH`` to download a
particular version.

We recommend users join the official PETSc :ref:`mailing lists <doc_mail>` to be submit
any questions they may have directly to the development team, be notified of new major
releases and bug-fixes, or to simply keep up to date with the current state of the
library.

Alternative Download
====================

Tarball contains only the source, identical to ``git`` download. Documentation available online.

- `petsc-lit-3.14.2.tar.gz <https://ftp.mcs.anl.gov/pub/petsc/release-snapshots/petsc-lite-3.14.2.tar.gz>`__

Tarball includes all documentation, recommended for offline use.

- `petsc-3.14.2.tar.gz <https://ftp.mcs.anl.gov/pub/petsc/release-snapshots/petsc-3.14.2.tar.gz>`__

To extract the sources use:

.. code-block:: console

   > gunzip -c petsc-<version number>.tar.gz | tar -xof -

Use mirror if GitLab and our primary download server are unavailable:

- `Primary server <https://ftp.mcs.anl.gov/pub/petsc/release-snapshots/>`__

- `Mirror <https://www.mcs.anl.gov/petsc/mirror/release-snapshots/>`__

.. Note::

   Older releases of PETSc are also available above. These should only be used for
   applications that have not been updated to the latest release. We urge you, whenever
   possible, to upgrade to the latest version of PETSc.

PETSc Development Repository
============================

You can work with the `development version of PETSc
<https://docs.petsc.org/en/latest/developers/index.html>`__, and decide when to update to
the latest code in the repository. This also facilitates easy submission of fixes and new
features to the development team.

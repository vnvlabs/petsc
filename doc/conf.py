# Configuration file for the Sphinx documentation builder.
#
# For information on options, see
#   http://www.sphinx-doc.org/en/master/config
#
# You may also find it helpful to run "sphinx-quickstart" in a scratch
# directory and read the comments in the automatically-generated conf.py file.

import os
import sys
import subprocess
import re
import datetime

sys.path.append(os.getcwd())
sys.path.append(os.path.abspath('./ext'))

import add_version_header
import build_classic_docs
import make_links_relative


if not os.path.isdir("images"):
    print("-----------------------------------------------------------------------------")
    print("ERROR")
    print("images directory does not seem to exist.")
    print("To clone the required repository, try")
    print("   make images")
    print("-----------------------------------------------------------------------------")
    raise Exception("Aborting because images missing")


# -- Project information -------------------------------------------------------

project = 'PETSc'
copyright = '1991-%d, UChicago Argonne, LLC and the PETSc Development Team' % datetime.date.today().year
author = 'The PETSc Development Team'

with open(os.path.join('..', 'include', 'petscversion.h'),'r') as version_file:
    buf = version_file.read()
    petsc_release_flag = re.search(' PETSC_VERSION_RELEASE[ ]*([0-9]*)',buf).group(1)
    major_version      = re.search(' PETSC_VERSION_MAJOR[ ]*([0-9]*)',buf).group(1)
    minor_version      = re.search(' PETSC_VERSION_MINOR[ ]*([0-9]*)',buf).group(1)
    subminor_version   = re.search(' PETSC_VERSION_SUBMINOR[ ]*([0-9]*)',buf).group(1)

    git_describe_version = subprocess.check_output(['git', 'describe', '--always']).strip().decode('utf-8')
    if petsc_release_flag == '0':
        version = git_describe_version
        release = git_describe_version
    else:
        version = '.'.join([major_version, minor_version])
        release = '.'.join([major_version,minor_version,subminor_version])


# -- General configuration -----------------------------------------------------

needs_sphinx='3.5'
nitpicky = True  # checks internal links. For external links, use "make linkcheck"
master_doc = 'index'
templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
highlight_language = 'c'
numfig = True

# -- Extensions ----------------------------------------------------------------

extensions = [
    'sphinx_copybutton',
    'sphinx_panels',
    'sphinxcontrib.bibtex',
    'sphinxcontrib.katex',
    'sphinxcontrib.rsvgconverter',
    'html5_petsc',
]

copybutton_prompt_text = '$ '

bibtex_bibfiles = ['petsc.bib']

# -- Options for HTML output ---------------------------------------------------

html_theme = 'pydata_sphinx_theme'

html_theme_options = {
    "icon_links": [
        {
            "name": "GitLab",
            "url": "https://gitlab.com/petsc/petsc",
            "icon": "fab fa-gitlab",
        },
    ],
    "use_edit_page_button": True,
    "footer_items": ["copyright", "sphinx-version", "last-updated"],
}

html_context = {
    "github_url": "https://gitlab.com",
    "github_user": "petsc",
    "github_repo": "petsc",
    "github_version": "release",
    "doc_path": "doc",
}

html_logo = os.path.join('images', 'logos', 'PETSc_TAO_logos', 'PETSc-TAO', 'web', 'PETSc-TAO_RGB.svg')
html_favicon = os.path.join('images', 'logos', 'PETSc_TAO_logos', 'PETSc', 'petsc_favicon.png')
html_last_updated_fmt = r'%Y-%m-%dT%H:%M:%S%z (' + git_describe_version + ')'



# -- Options for LaTeX output --------------------------------------------------
latex_engine = 'xelatex'

# How to arrange the documents into LaTeX files, building only the manual.
latex_documents = [
        ('docs/manual/index', 'manual.tex', 'PETSc/TAO Users Manual', author, 'manual', False)
        ]

latex_additional_files = [
    'images/docs/manual/anl_tech_report/ArgonneLogo.pdf',
    'images/docs/manual/anl_tech_report/ArgonneReportTemplateLastPage.pdf',
    'images/docs/manual/anl_tech_report/ArgonneReportTemplatePage2.pdf',
    'docs/manual/anl_tech_report/first.inc',
    'docs/manual/anl_tech_report/last.inc',
]

latex_elements = {
    'maketitle': r'\newcommand{\techreportversion}{%s}' % version +
r'''
\input{first.inc}
''',
    'printindex': r'''
\printindex
\input{last.inc}
''',
    'fontpkg': r'''
\setsansfont{DejaVu Sans}
\setmonofont{DejaVu Sans Mono}
''',
    'tableofcontents' : r''
}


# -- Setup and event callbacks -------------------------------------------------

# Trigger a build of the "classic" docs
def _build_classic_docs(app):
    build_classic_docs.main()


def builder_init_handler(app):
    _build_classic_docs(app)


def _add_version_header(app, exception):
    if exception is None and app.builder.name.endswith('html'):
        print("============================================")
        print("    Adding version to classic man pages, from conf.py")
        print("============================================")
        add_version_header.add_version_header(os.path.join(app.outdir, "docs", "manualpages"), release)


def _copy_classic_docs(app, exception):
    if exception is None and app.builder.name.endswith('html'):
        print("============================================")
        print("    Copying classic docs from conf.py       ")
        print("============================================")
        build_classic_docs.copy_classic_docs(app.outdir)


def _fix_links(app, exception):
    if exception is None and app.builder.name.endswith('html'):
        print("============================================")
        print("    Fixing relative links from conf.py      ")
        print("============================================")
        make_links_relative.make_links_relative(app.outdir)


def build_finished_handler(app, exception):
    _copy_classic_docs(app, exception)
    _fix_links(app, exception)
    _add_version_header(app, exception)
    if app.builder.name == 'html':
        print("==========================================================================")
        print("    open %s/index.html in your browser to view the documentation " % app.outdir)
        print("==========================================================================")


def setup(app):
    app.connect('builder-inited', builder_init_handler)
    app.connect('build-finished', build_finished_handler)
    app.add_css_file('css/pop-up.css')

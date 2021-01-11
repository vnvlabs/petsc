import config.package

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    self.download     = []
    self.functions    = []
    self.includes     = ['VnV.h']
    self.liblist      = [['libinjection.so']]
    self.useddirectly = 1
    return



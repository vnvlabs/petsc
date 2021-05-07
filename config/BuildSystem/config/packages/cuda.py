import config.package
import os

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    self.minversion        = '7.5'
    self.versionname       = 'CUDA_VERSION'
    self.versioninclude    = 'cuda.h'
    self.requiresversion   = 1
    self.functions         = ['cublasInit', 'cufftDestroy']
    self.includes          = ['cublas.h','cufft.h','cusparse.h','cusolverDn.h','thrust/version.h']
    self.liblist           = [['libcufft.a', 'libcublas.a','libcudart.a','libcusparse.a','libcusolver.a'],
                              ['cufft.lib','cublas.lib','cudart.lib','cusparse.lib','cusolver.lib']]
    self.precisions        = ['single','double']
    self.cxx               = 0
    self.complex           = 1
    self.hastests          = 0
    self.hastestsdatafiles = 0
    self.functionsDefine   = ['cusolverDnDpotri']
    return

  def setupHelp(self, help):
    import nargs
    config.package.Package.setupHelp(self, help)
    help.addArgument('CUDA', '-with-cuda-gencodearch', nargs.ArgString(None, None, 'Cuda architecture for code generation, for example 70, (this may be used by external packages), use all to build a fat binary for distribution'))
    return

  def __str__(self):
    output  = config.package.Package.__str__(self)
    if hasattr(self,'gencodearch'):
      output += '  CUDA SM '+self.gencodearch+'\n'
    return output

  def setupDependencies(self, framework):
    config.package.Package.setupDependencies(self, framework)
    self.scalarTypes  = framework.require('PETSc.options.scalarTypes',self)
    self.compilers    = framework.require('config.compilers',self)
    self.thrust       = framework.require('config.packages.thrust',self)
    self.odeps        = [self.thrust] # if user supplies thrust, install it first
    return

  def getSearchDirectories(self):
    import os
    self.pushLanguage('CUDA')
    petscNvcc = self.getCompiler()
    self.popLanguage()
    self.getExecutable(petscNvcc,getFullPath=1,resultName='systemNvcc')
    if hasattr(self,'systemNvcc'):
      self.nvccDir = os.path.dirname(self.systemNvcc)
      self.cudaDir = os.path.split(self.nvccDir)[0]
      yield self.cudaDir
    return

  def checkSizeofVoidP(self):
    '''Checks if the CUDA compiler agrees with the C compiler on what size of void * should be'''
    self.log.write('Checking if sizeof(void*) in CUDA is the same as with regular compiler\n')
    size = self.types.checkSizeof('void *', (8, 4), lang='CUDA', save=False)
    if size != self.types.sizes['void-p']:
      raise RuntimeError('CUDA Error: sizeof(void*) with CUDA compiler is ' + str(size) + ' which differs from sizeof(void*) with C compiler')
    return

  def checkThrustVersion(self,minVer):
    '''Check if thrust version is >= minVer '''
    include = '#include <thrust/version.h> \n#if THRUST_VERSION < ' + str(minVer) + '\n#error "thrust version is too low"\n#endif\n'
    self.pushLanguage('CUDA')
    valid = self.checkCompile(include)
    self.popLanguage()
    return valid

  def configureTypes(self):
    import config.setCompilers
    if not self.getDefaultPrecision() in ['double', 'single']:
      raise RuntimeError('Must use either single or double precision with CUDA')
    self.checkSizeofVoidP()
    if not self.thrust.found and self.scalarTypes.scalartype == 'complex': # if no user-supplied thrust, check the system's complex ability
      if not self.compilers.cxxdialect in ['C++11','C++14']:
        raise RuntimeError('CUDA Error: Using CUDA with PetscComplex requirs a C++ dialect at least cxx11. Use --with-cxx-dialect=xxx to specify a proper one')
      if not self.checkThrustVersion(100908):
        raise RuntimeError('CUDA Error: The thrust library is too low to support PetscComplex. Use --download-thrust or --with-thrust-dir to give a thrust >= 1.9.8')
    if self.compilers.cxxdialect in ['C++11','C++14']: #nvcc is a C++ compiler so it is always good to add -std=xxx. It is even crucial when using thrust complex (see MR 2822)
      self.setCompilers.CUDAFLAGS += ' -std=' + self.compilers.cxxdialect.lower()
    return

  def versionToStandardForm(self,ver):
    '''Converts from CUDA 7050 notation to standard notation 7.5'''
    return ".".join(map(str,[int(ver)//1000, int(ver)//10%10]))

  def checkNVCCDoubleAlign(self):
    if 'known-cuda-align-double' in self.argDB:
      if not self.argDB['known-cuda-align-double']:
        raise RuntimeError('CUDA error: PETSC currently requires that CUDA double alignment match the C compiler')
    else:
      typedef = 'typedef struct {double a; int b;} teststruct;\n'
      cuda_size = self.types.checkSizeof('teststruct', (16, 12), lang='CUDA', codeBegin=typedef, save=False)
      c_size = self.types.checkSizeof('teststruct', (16, 12), lang='C', codeBegin=typedef, save=False)
      if c_size != cuda_size:
        raise RuntimeError('CUDA compiler error: memory alignment doesn\'t match C compiler (try adding -malign-double to compiler options)')
    return

  def configureLibrary(self):
    config.package.Package.configureLibrary(self)
    self.checkNVCCDoubleAlign()
    self.configureTypes()
    # includes from --download-thrust should override the prepackaged version in cuda - so list thrust.include before cuda.include on the compile command.
    if self.thrust.found:
      self.log.write('Overriding the thrust library in CUDAToolkit with a user-specified one\n')
      self.include = self.thrust.include+self.include

    if 'with-cuda-gencodearch' in self.framework.clArgDB:
      self.gencodearch = self.argDB['with-cuda-gencodearch']
    else:
      import os
      self.pushLanguage('CUDA')
      petscNvcc = self.getCompiler()
      self.popLanguage()
      self.getExecutable(petscNvcc,getFullPath=1,resultName='systemNvcc')
      if hasattr(self,'systemNvcc'):
        cudaDir = os.path.dirname(os.path.dirname(self.systemNvcc))
        dq = os.path.join(cudaDir,'extras','demo_suite')
        self.getExecutable('deviceQuery',path = dq)
        if hasattr(self,'deviceQuery'):
          try:
            (out, err, ret) = Configure.executeShellCommand(self.deviceQuery + ' | grep "CUDA Capability"',timeout = 60, log = self.log, threads = 1)
          except:
            self.log.write('deviceQuery failed\n')
          else:
            try:
              out = out.split('\n')[0]
              sm = out[-3:]
              self.gencodearch = str(int(10*float(sm)))
            except:
              self.log.write('Unable to parse CUDA capability\n')

    if hasattr(self,'gencodearch'):
      if self.gencodearch == 'all':
        for gen in ['52','60','61','70','75']:
          self.setCompilers.CUDAFLAGS += ' -gencode arch=compute_'+gen+',code=sm_'+gen+' '
          self.log.write(self.setCompilers.CUDAFLAGS+'\n')
      else:
        self.setCompilers.CUDAFLAGS += ' -gencode arch=compute_'+self.gencodearch+',code=sm_'+self.gencodearch+' '

    self.addDefine('HAVE_CUDA','1')
    if not self.version_tuple:
      self.checkVersion(); # set version_tuple
    if self.version_tuple[0] >= 11:
      self.addDefine('HAVE_CUDA_VERSION_11PLUS','1')
    return

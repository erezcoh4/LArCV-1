from .. import larcv

import abc

class PlotImage(object):
    __metaclass__ = abc.ABCMeta

    def __init__(self,img_v):
        
        self.imgs   = [ img_v[i] for i in xrange(img_v.size()) ]
        self.img_v  = [ larcv.as_ndarray(img) for img in self.imgs  ]

        self.__create_mat__()
        self.plot_mat_t = None


    @abc.abstractmethod
    def __create_mat__(self):
        """create plot_mat meaningfully"""


    @abc.abstractmethod
    def __threshold_mat__(self,imin,imax):
        """transform plot_mat meaningfully"""

        
    def treshold_mat(self,imin,imax):
        self.__threshold_mat__(imin,imax)

        return self.plot_mat_t
    
        
        
        
        
        
        

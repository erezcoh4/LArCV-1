from larcv import larcv
import sys

io=larcv.IOManager(larcv.IOManager.kREAD)

io.add_in_file(sys.argv[1])

io.initialize()

print

for x in xrange(larcv.kProductUnknown):

    producers=io.producer_list(x)

    print 'Producer type:',x,'(aka',larcv.ProductName(x),')...'
    for p in producers: print p
    print


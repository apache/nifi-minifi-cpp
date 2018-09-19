#Define bridge streaming lib
LIB=libuuid.a

# define the C++ compiler to use
CC=$(CROSS_COMPILE)-gcc

COMPILERFLAGS = -O0 -ggdb -g3 -fexceptions -Wno-write-strings -fvisibility=hidden -fPIC

# define any compile-time flags
CFLAGS= -g -Wall -rdynamic  $(COMPILERFLAGS)

# define any directories containing header files other than /usr/include
#
INCLUDES=-I..

# define library paths in addition to /usr/lib
#   if I wanted to include libraries not in /usr/lib I'd specify
#   their path using -Lpath, something like:
LFLAGS=

# define any libraries to link into executable:
#   if I want to link in libraries (libx.so or libx.a) I use the -llibname 
#   option, something like (this will link in libmylib.so and libm.so:
LIBS=-lpthread

# define the CPP source files
SRCS=clear.c  compare.c  copy.c  gen_uuid.c  isnull.c  pack.c  parse.c  unpack.c  unparse.c  m_uuidime.c

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS=$(SRCS:.c=.o)

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

all:    $(LIB) tests

#create static library
$(LIB): $(OBJS) 
		rm -f  $(LIB)
		$(CROSS_COMPILE)-ar -cvq $(LIB) $(OBJS) 	

# this is a suffix replacement rule for building .o's from .cpp's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .cpp file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
		$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
		$(RM) *.o $(LIB)
		find ./ -iname "*.o" -exec $(RM) {} \;

tests: $(LIB) ./tst_uuid.c
	$(CC) $(CFLAGS) $(INCLUDES) ./tst_uuid.c  $(LIB) -o ./tst_uuid

depend: $(SRCS)
		makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it


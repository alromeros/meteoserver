# ********************************************************* #
#                                                           #
#                                                           #
#    Makefile: meteoserver                                  #
#                                                           #
#    By: Alvaro Romero <alvromero96@gmail.com>              #
#                                                           #
#    Created: 2021/11/24                                    #
#                                                           #
# ********************************************************* #

# Compilation / rule options
CC		= 	gcc
CFLAGS	= 	-Werror -Wextra
DEL		= 	rm -rf
PTHREAD	=	-pthread

ifeq ($(DEBUG), 1)
CFLAGS	+= -g
endif

# Source, object and binary files
SRC		= 	main.c \
			crypto.c \
			requestQueue.c \
			lruCache.c \
			requestMonitor.c
OBJ		= 	$(addprefix $(OBJDIR)/,$(SRC:.c=.o))
NAME	= 	meteoserver
INC		= 	meteoserver.h

# Directories
SRCDIR	= 	./src
SRCDIRS	= 	./utils \
			./dataStructures \
			./requestMonitor
INCDIR	= 	./inc
OBJDIR	= 	./obj
VPATH	=	$(addprefix $(SRCDIR)/,$(SRCDIRS)) $(SRCDIR)

# Rules
all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -I $(INCDIR) $(PTHREAD) -o $(NAME)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR) \
	&& $(CC) $(CFLAGS) -I $(INCDIR) $(PTHREAD) -o $@ -c $<

clean:
	$(DEL) $(OBJ) $(OBJDIR)

fclean: clean
	$(DEL) $(NAME)

re: fclean all

test: all
	@echo "\n\033[32mTesting with 400 requests: \033[0m\n" && sleep 2 \
	&& ./test/stress_test.sh 100 &	
	   @./$(NAME) -p 100 -C 10

.PHONY: test re fclean clean

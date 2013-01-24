OBJS=cgroup.o grouper.o main.o utils.o nl_proc.o
%.o:%.c
	gcc $< -c -o $@ -g
cgrouper:$(OBJS)
	gcc $(OBJS) -o $@ -lprocps -lev -g
.PHONY:clean
clean:
	-rm $(OBJS) cgrouper

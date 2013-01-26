#include <sys/queue.h>
#include <sys/mount.h>
#include <limits.h>
#include <mntent.h>

#include "cgroup.h"
struct cgroup_entry{
	char *type;
	char *mp;
	LIST_ENTRY(cgroup_entry) next;
};

LIST_HEAD(cgroup_entries, cgroup_entry) cgroup_head;

static void add_mount(const char *type, const char *mp){
	struct cgroup_entry *cent;
	LIST_FOREACH(cent, &cgroup_head, next)
		if(strcmp(type, cent->type) == 0){
			if(!cent->mp){
				cent->mp = malloc(strlen(mp)+1);
				/* Make sure the mount point doesn't end with '/' */
				strcpy(cent->mp, mp);
				if(mp[strlen(mp)-1] == '/')
					cent->mp[strlen(mp)-1] = 0;
			}
			return;
		}
}
static struct cgroup_entry *option_contain_cgroup_type(const char *opts){
	struct cgroup_entry *cent;
	char *tmp_opts = malloc(strlen(opts)+1);
	LIST_FOREACH(cent, &cgroup_head, next){
		strcpy(tmp_opts, opts);
		char *opt = strtok(tmp_opts, ",");
		while(opt){
			if(strcmp(opt, cent->type) == 0){
				free(tmp_opts);
				return cent;
			}
			opt = strtok(NULL, ",");
		}
	}
	free(tmp_opts);
	return NULL;
}
static int cgroup_get_mount(){
	FILE *cgroup_types_file = fopen("/proc/cgroups", "r");
	char *line = NULL;
	size_t size;
	int ret;
	ret = getdelim(&line, &size, '\t', cgroup_types_file);
	while(ret != -1){
		if(line[0] != '#'){
			if(line[strlen(line)-1] == '\t')
				line[strlen(line)-1] = 0;
			struct cgroup_entry *new = zmalloc(sizeof(struct cgroup_entry));
			new->type = zmalloc(ret);
			strcpy(new->type, line);
			new->mp = NULL;
			LIST_INSERT_HEAD(&cgroup_head, new, next);
		}
		ret = getline(&line, &size, cgroup_types_file);
		if(ret == -1)
			break;
		ret = getdelim(&line, &size, '\t', cgroup_types_file);
	}
	if(line)
		free(line);

	struct mntent *entry;
	FILE *mounts = setmntent("/proc/mounts", "r");
	while((entry = getmntent(mounts)) != NULL){
		if(strcmp(entry->mnt_type, "cgroup") == 0){
			struct cgroup_entry *cent = option_contain_cgroup_type(entry->mnt_opts);
			if(cent){
				int len = strlen(entry->mnt_dir);
				cent->mp = malloc(len+1);
				strcpy(cent->mp, entry->mnt_dir);
				if(entry->mnt_dir[len-1] == '/')
					cent->mp[len-1] = 0;
			}
		}
	}
	return EXIT_SUCCESS;
}
static struct cgroup_entry *cgroup_is_mounted(const char *type){
	struct cgroup_entry *entry;
	LIST_FOREACH(entry, &cgroup_head, next)
		if(strcmp(type, entry->type) == 0)
			return entry;
	return NULL;
}
const char *cgroup_mount(const char *type){
	struct cgroup_entry *entry;
	if((entry = cgroup_is_mounted(type)))
		return entry->mp;
	char *target = malloc(PATH_MAX);
	int len = snprintf(target, PATH_MAX, "/sys/fs/cgroup/%s", type);
	if(len >= PATH_MAX){
		printf("cgroup name too long\n");
		return NULL;
	}
	int ret = mount("cgroup", target, "cgroup", MS_NODEV|MS_NOEXEC|MS_NOSUID|MS_RELATIME, type);
	if(ret == -1){
		free(target);
		return NULL;
	}
	add_mount(type, target);
	return target;
}
struct cgroup{
	char *full_path;
};
struct cgroup *cgroup_get(const char *type, const char *path, bool create){
	struct cgroup_entry *cg = cgroup_is_mounted(type);
	const char *mp;
	if(!cg){
		if(create)
			mp = cgroup_mount(type);
		else
			return NULL;
	}else
		mp = cg->mp;
	char *tmp_path = malloc(strlen(path)+1);
	char *tmp_full_path = malloc(strlen(mp)+strlen(path)+2);
	int path_len = strlen(mp);
	strcpy(tmp_path, path);
	strcpy(tmp_full_path, mp);
	struct cgroup *cgr = malloc(sizeof(struct cgroup));
	char *dir = strtok(tmp_path, "/");
	while(dir){
		if(strlen(dir) == 0)
			goto next;
		tmp_full_path[path_len++] = '/';
		strcpy(tmp_full_path+path_len, dir);
		struct stat sbuf;
		if(stat(tmp_full_path, &sbuf) == -1){
			if(errno == ENOENT){
				if(create)
					mkdir(tmp_full_path, 0755);
				else
					goto out;
			}else
				goto out;
		}
	next:
		dir = strtok(NULL, "/");
	}
	cgr->full_path = tmp_full_path;
	free(tmp_path);
	return cgr;
out:
	free(tmp_full_path);
	free(tmp_path);
	return NULL;
}
void cgroup_put(struct cgroup *cgr){
	free(cgr->full_path);
}
int cgroup_attach(struct cgroup *cgr, int pid){
	char *tasks_filename;
	asprintf(&tasks_filename, "%s/tasks", cgr->full_path);
	FILE *tasks_file = fopen(tasks_filename, "w");
	if(!tasks_file){
		free(tasks_filename);
		return EXIT_FAILURE;
	}
	int ret = fprintf(tasks_file, "%d\n", pid);
	fclose(tasks_file);
	free(tasks_filename);
	if(ret < 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
int cgroup_init(){
	return cgroup_get_mount();
}

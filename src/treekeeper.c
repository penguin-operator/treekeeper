/*
 * thanks https://superjamie.github.io/2022/08/06/ncursesw
 * helped with cyrillic and other unicode stuff
 */
#define _XOPEN_SOURCE_EXTENDED

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include <string.h>
#include <locale.h>
#include <ncursesw/curses.h>
#include "md5-c/md5.c"

// #define ANSI_BRIGHT_YELLOW_BOLD "\e[93;1m"
// #define ANSI_BRIGHT_GREEN_BOLD "\e[92;1m"
// #define ANSI_NORMAL "\e[0m"
#define DRIVE_MARK_NAME_SIZE 16
#define DRIVE_MARK_PATH_SIZE 500
#define DRIVE_MARK_MAX 250
#define TREEKEEPER_MAX_ENTRIES 120000000

struct drive_mark {char name [DRIVE_MARK_NAME_SIZE]; char path [DRIVE_MARK_PATH_SIZE]; };
struct entry {long pos; int xlink;};

char *p_buffer = 0;
size_t p_buffer_size = 0;
char *p_drive_name;
long p_seen_at [200]; /* use this vars for display */
int p_seen_at_i;
int p_uniq;

void parse_entry_for_display (int i, struct entry *array, long array_i, FILE *f, FILE *fx) {
	p_uniq = 0;
	p_seen_at_i = 0;
	fseek(f, array[i].pos, SEEK_SET);
	ssize_t ret = getline(&p_buffer, &p_buffer_size, f);
	
	if (-1 == array[i].xlink) {
		p_uniq = 1;
		return;
	}
	
	memset(p_seen_at, 0, sizeof p_seen_at);
	
	fseek(fx, array[i].xlink, SEEK_SET);
	char *b;
	char md5_prev [33];
	int md5_prev_set = 0;
	while (1) {
		long b_size = 0; /* allocate a new buffer */
		ret = getline(&b, &b_size, fx); /* read the whole line from xlink file */
		if (-1 != ret) {
			char *md5 = strtok(b, " ");
			char *num_string = strtok(0, " ");
			if (md5_prev_set) {
				ret = memcmp(md5, md5_prev, 32);
				if (ret) break;
			}
			memcpy(md5_prev, md5, 32);
			md5_prev_set = 1;
			long xlink_num = strtol(num_string, 0, 10);
			if (xlink_num < 0) break;
			//fprintf(stdout, "found xlink num %li\n", xlink_num);
			p_seen_at[p_seen_at_i] = xlink_num;
			p_seen_at_i++;
			if (p_seen_at_i >= DRIVE_MARK_MAX) break;
		} else break;
	}
	
	free(b);
}

int main (int argc, char **argv) {
	int ret = system("which tree");
	if (ret) return 1;
	ret = system("which sort");
	if (ret) return 1;
	
	char *database_file_name = "~/treekeeper_database_file";
	wordexp_t exp_result;
    wordexp(database_file_name, &exp_result, 0);
    char *database_file_name_resolved = exp_result.we_wordv[0]; /* just get the home directory */
	
	int create_database_no_prompt = 1;
	
	FILE *database_file = fopen(database_file_name_resolved, "r");
	if (!database_file) {
		/* if (!create_database_no_prompt) {
			fprintf(stderr, ANSI_BRIGHT_GREEN_BOLD "Press 'y' and then 'enter' to start scanning and then enter ncurses mode...  " ANSI_NORMAL);
			char l [30];
			char *p = l;
			size_t len = sizeof(l);
			getline(&p, &len, stdin);
			if ('y' == l[0] || 'Y' == l[0]) {
			} else errx(1, "Exiting");
		} */
		database_file = fopen(database_file_name_resolved, "a");
		if (!database_file) return 1;
		fclose(database_file);
		database_file = 0; /* will open it again soon */
	}
	
	ret = system("tree -sfipa /home /mnt /media > /tmp/treekeeper_tree_output");
	if (ret) return 1;

	FILE *tree_output = fopen("/tmp/treekeeper_tree_output", "r");
	if (!tree_output) return 1;
	
	struct drive_mark drive_mark_array [DRIVE_MARK_MAX];
	int drive_mark_array_i = 0;
	
	char *buffer = 0; /* ask getline to allocate a new buffer */
	size_t buffer_size = 0;
	
	int drive_mark_counter = 0;
	
	while (-1 != getline(&buffer, &buffer_size, tree_output)) { /* read file line by line */
		if ('-' != buffer[1]) continue; /* ignore directories, named pipes, fifo, sockets, links */
		char *pos = strstr(buffer, "treekeeper_drive_");
		if (!pos) continue;
		
		char *running_i = pos+17;
		while ('\0' != *running_i) {
			if ('\n' == *running_i) *running_i = 0; /* remove carret return */
			running_i++;
		}
		
		drive_mark_counter++;
		strncpy(drive_mark_array[drive_mark_array_i].name, pos+17, DRIVE_MARK_NAME_SIZE);
		drive_mark_array[drive_mark_array_i].name[DRIVE_MARK_NAME_SIZE - 1] = '\0';
		
		drive_mark_array[drive_mark_array_i].path[0] = '\0';
		pos[0] = '\0'; /* mark path end here */
		if ('.' == pos[-1]) pos[-1] = '\0'; /* hidden file also works */
		char *pos2 = buffer;
		while (*pos2) {
			if ('/' == *pos2) {
				strncpy(drive_mark_array[drive_mark_array_i].path, pos2, DRIVE_MARK_PATH_SIZE);
				drive_mark_array[drive_mark_array_i].path[DRIVE_MARK_PATH_SIZE - 1] = '\0';
				break;
			}
			pos2++;
		}

		int i;
		int more_then_once = 1;
		for (i = 0; i < drive_mark_array_i; i++) {
			if (0 == strcmp(drive_mark_array[drive_mark_array_i].name, drive_mark_array[i].name)) {
				more_then_once++;
			} /* store it anyway */
		}
		drive_mark_array_i++;
		if (drive_mark_array_i >= DRIVE_MARK_MAX) break;
	}
	
	// if (!drive_mark_counter) warnx(ANSI_BRIGHT_YELLOW_BOLD "this program relies on special named files for identifying removable media. Consider creating a file named \"treekeeper_drive_mydrive\" or \".treekeeper_drive_mydrive\" at the removable media root directory" ANSI_NORMAL);
	
	char cmd_text [2400];
	snprintf(cmd_text, sizeof (cmd_text), "mv %s %s_previous", database_file_name_resolved, database_file_name_resolved);
	system(cmd_text); /* move database file */
	
	database_file = fopen(database_file_name_resolved, "w");
	if (!database_file) return 1;
	
	rewind(tree_output);
	while (-1 != getline(&buffer, &buffer_size, tree_output)) { /* read file line by line */
		if ('-' != buffer[1]) continue; /* ignore directories, named pipes, fifo, sockets, links */
		/* try to decide drive based on path */
		char *deliiter_pos = strstr(buffer, "/");
		if (!deliiter_pos) continue;
		char *drive_name = "n-a";
		int i;
		for (i = 0; i < drive_mark_array_i; i++) {
			char *needle_pos = strstr(buffer, drive_mark_array[i].path);
			if (needle_pos == deliiter_pos) drive_name = drive_mark_array[i].name; /* found a match */
		}
		fprintf(database_file, "%s\t%s", drive_name, buffer); /* write into database */
		
	}
	fclose(tree_output);
	char prev_database_filename [1900];
	snprintf(prev_database_filename, sizeof prev_database_filename, "%s_previous", database_file_name_resolved);
	FILE *database_prev = fopen(prev_database_filename, "r");
	if (!database_prev) return 1;
	
	while (-1 != getline(&buffer, &buffer_size, database_prev)) {
		int i;
		int this_drive_is_updating = 0;
		for (i = 0; i < drive_mark_array_i; i++) {
			char *needle_pos = strstr(buffer, drive_mark_array[i].name);
			if (needle_pos == buffer) { /* trying to find drive name in the beginning of the line */
				this_drive_is_updating = 1;
				break;
			}
		}
		if (!this_drive_is_updating) fprintf(database_file, "%s", buffer); /* write entry as it is */
		/* only not connected drives go through */
	}
	
	fclose(database_prev);
	fclose(database_file);
	
	snprintf(cmd_text, sizeof cmd_text, "rm %s /tmp/treekeeper_tree_output", prev_database_filename);
	system(cmd_text);

	/* read new database into memory */
	/* and also generate a hash table */
	struct entry *array_of_entries = malloc(TREEKEEPER_MAX_ENTRIES * sizeof(struct entry));
	if (!array_of_entries) return 1;
	long array_of_entries_i = 0;

	database_file = fopen(database_file_name_resolved, "r");
	if (!database_file) return 1;
	
	char database_xlink_filename [1900];
	snprintf(database_xlink_filename, sizeof database_xlink_filename, "%s_xlink", database_file_name_resolved);
	FILE *database_xlink = fopen(database_xlink_filename, "w");
	if (!database_xlink) return 1;
	
	long database_pos;
	while (1) {
		database_pos = ftell(database_file);
		if (-1 == getline(&buffer, &buffer_size, database_file)) break; /* read line */
		char *drive_name = strtok(buffer, "\t");
		if (!drive_name) continue;
		if (!strtok(0, " ")) continue;
		char *size_string = strtok(0, "]");
		if (!size_string) continue;
		
		char *file_name = 0;
		while (1) {
			char *t = strtok(0, "/");
			if (!t) break;
			file_name = t;
		}
		
		if (!file_name) continue;
		
		char name_and_size [1800];
		snprintf(name_and_size, sizeof name_and_size, "%s\t%s", size_string, file_name); /* this is debug info, there is no need to put it into xlink file */
		char result [16];
		md5String(name_and_size, result);
		/* for(unsigned int e = 0; e < 16; ++e) {
			fprintf(stderr, "%02x", (unsigned char) (result[e]));
		}
		fprintf(stderr, "\n"); */
		
		array_of_entries[array_of_entries_i].pos = database_pos;
		array_of_entries[array_of_entries_i].xlink = -1;
		
		for(unsigned int e = 0; e < 16; ++e) {
			fprintf(database_xlink, "%02x", (unsigned char) (result[e]));
		}
		fprintf(database_xlink, " %li %s", array_of_entries_i, name_and_size); /* write hash to file */
		array_of_entries_i++;
		if (array_of_entries_i >= TREEKEEPER_MAX_ENTRIES) {
			break;
		}
	}

	/* system sort */
	
	fclose(database_xlink);
	snprintf(cmd_text, sizeof cmd_text, "mv %s %s_unsorted", database_xlink_filename, database_xlink_filename);
	system(cmd_text);
	snprintf(cmd_text, sizeof cmd_text, "sort %s_unsorted > %s", database_xlink_filename, database_xlink_filename);
	system(cmd_text);

	database_xlink = fopen(database_xlink_filename, "r");
	int first_hash_written = 0;
	long num_previous = 0;
	long xlink_pos_prev = 0;
	char hash_previous [33];
	memset(hash_previous, 0, 32);
	while (1) {
		long xlink_pos = ftell(database_xlink);
		if (-1 == getline(&buffer, &buffer_size, database_xlink)) break;
		char *md5 = strtok(buffer, " ");
		char *num_str = strtok(0, " ");
		if (!num_str) continue;
		long num = strtol(num_str, 0, 10);
		hash_previous[32] = 0;
		ret = memcmp(md5, hash_previous, 32);
		if (0 == ret) {
			if (!first_hash_written) {
				/* try to write to entry for first occurance */
				if (num_previous >= array_of_entries_i) continue;
				if (num_previous < 0) continue; /* boundary checks */
				array_of_entries[num_previous].xlink = xlink_pos_prev;
				first_hash_written = 1;
			}
			if (num >= array_of_entries_i) continue;
			if (num < 0) continue; /* boundary checks */
			array_of_entries[num].xlink = xlink_pos_prev;
		} else first_hash_written = 0;
		
		memcpy(hash_previous, md5, 32);
		num_previous = num;
		if (!first_hash_written) xlink_pos_prev = xlink_pos;
		
		/* find repeating hashes */
		/* write offset to the entry, so later code can read those crosslinks  */
	}

	/* remove unsorted xlink */
	snprintf(cmd_text, sizeof cmd_text, "rm %s_unsorted", database_xlink_filename);
	system(cmd_text);

	/* memory dump here */
	// for (long i = 0; i < array_of_entries_i; i++) {
	// 	parse_entry_for_display(i, array_of_entries, array_of_entries_i, database_file, database_xlink);
	// 	printw("%li ", i);
	// 	addstr(p_uniq ? " unique " : "        ");
	// 	addstr(p_buffer);
	// 	if (p_seen_at_i > 0) {
	// 		for (int ii = 0; ii < p_seen_at_i; ii++) {
	// 			if (p_seen_at[ii] == i) continue;
	// 			fseek(database_file, array_of_entries[p_seen_at[ii]].pos, SEEK_SET);
	// 			char buf_mark [DRIVE_MARK_NAME_SIZE + 1];
	// 			fread(buf_mark, DRIVE_MARK_NAME_SIZE, 1, database_file);
	// 			buf_mark[DRIVE_MARK_NAME_SIZE] = '\0';
	// 			char *mark = strtok(buf_mark, "\t");
	// 			printw(" (%s %li)", mark, p_seen_at[ii]);
	// 		}
	// 	}
	// }

	setlocale(LC_ALL, "");
	initscr();
	noecho();
	keypad(stdscr, 1);
	start_color();
	curs_set(0);

	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);

	long height, width;
	long y = 0, x = 0, p = 0;
	char c = 0;
	getmaxyx(stdscr, height, width);

	while (1) {
		for (long i = 0; i < height - 1; i++) {
			if (i > array_of_entries_i - y - 1) break;
			if (p == i + y) attron(A_UNDERLINE);

			parse_entry_for_display(i + y, array_of_entries, array_of_entries_i, database_file, database_xlink);

			attron(COLOR_PAIR(3));
			mvprintw(i, 1, "%li", i + y);
			attroff(COLOR_PAIR(3));
			attron(COLOR_PAIR(2));
			addstr(p_uniq ? " unique " : "        ");
			attroff(COLOR_PAIR(2));
			addstr(p_buffer);

			if (p == i + y) attroff(A_UNDERLINE);
		}
		/* clear the rest of previous prints */
		for (long i = 0; i < height - 1; i++) mvaddch(i, 0, ' ');

		parse_entry_for_display(p, array_of_entries, array_of_entries_i, database_file, database_xlink);

		attron(COLOR_PAIR(2) | A_BOLD);
		mvaddch(p - y, 0, '>');
		attroff(COLOR_PAIR(2) | A_BOLD);

		move(height - 1, 0);
		clrtoeol();
		attron(A_BOLD);
		mvaddstr(height - 1, 0, p_buffer+30);
		attroff(A_BOLD);

		// keybinds
		c = getch();
		if (c == 3) { // up
			p --;
			if (p < y) y --;
		} else if (c == 2) { // down
			p ++;
			if (p >= y + height - 1) y ++;
		} else if (c == 81) { // shift + up
			y --;
			p --;
		} else if (c == 80) { // shift + down
			y ++;
			p ++;
		} else if (c == 83) { // pageup
			y -= height - 1;
			p -= height - 1;
		} else if (c == 82) { // pagedown
			y += height - 1;
			p += height - 1;
		} else if (c == 6) { // home
			y = 0;
			p = 0;
		} else if (c == 104) { // end
			y = array_of_entries_i - height + 1;
			p = array_of_entries_i - 1;
		} else if (c == 10 && !p_uniq) { // enter
			clear();
			move(0, 0);
			attron(A_BOLD);
			printw("%li %s%i duplicates", p, p_buffer, p_seen_at_i - 1);
			attroff(A_BOLD);

			attron(A_BOLD | COLOR_PAIR(3));
			for (int i = 0; i < p_seen_at_i; i++) {
				if (p_seen_at[i] == p) continue;
				fseek(database_file, array_of_entries[p_seen_at[i]].pos, SEEK_SET);
				char buf_mark [DRIVE_MARK_NAME_SIZE + 1];
				fread(buf_mark, DRIVE_MARK_NAME_SIZE, 1, database_file);
				buf_mark[DRIVE_MARK_NAME_SIZE] = '\0';
				char *mark = strtok(buf_mark, "\t");
				printw(" (%s %li)", mark, p_seen_at[i]);
			}
			attroff(A_BOLD | COLOR_PAIR(3));

			attron(A_BOLD);
			mvaddstr(height - 1, (width - 23) / 2, " press any key to quit ");
			attroff(A_BOLD);
			getch();
		}

		if (y < 0) y = 0;
		if (y > array_of_entries_i - height + 1) y = array_of_entries_i - height + 1;
		if (p < 0) p = 0;
		if (p > array_of_entries_i - 1) p = array_of_entries_i - 1;
	}

	getch();
	endwin();

	return 0;
}

#define _XOPEN_SOURCE 500 /* glibc2 needs this */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

struct plant {
	/* input */
	char		*name;
	unsigned int	num_plants_to_harvest;
	unsigned int	num_weeks_indoors;
	unsigned int	num_weeks_until_indoor_separation;
	struct tm	outdoor_planting_date;
	unsigned int	num_weeks_until_outdoor_separation;
	unsigned int	days_to_harvest;
	float		germination_rate;
	unsigned int	min_days_to_sprout;
	float		avg_days_to_sprout;
	unsigned int	max_days_to_sprout;
	unsigned int	harvest_removes_plant; /* 0 = false; non-zero = true */
	/* output */
	/* XXX: These could be an array with an enum. */
	struct tm	seeding_date;
	struct tm	sprouting_date;
	struct tm	last_chance_sprouting_date;
	struct tm	indoor_separation_date;
	struct tm	hardening_off_date;
	struct tm	outdoor_separation_date;
	struct tm	harvest_date;
};

struct plant_date {
	struct tm	*date;
	char		*summary;
	char		*description;
};

struct date_list {
	struct plant_date	*cal_entry;
	struct date_list	*next;
};

#define MAX_NAME_LENGTH	500

int skip_comment_lines(FILE *fp)
{
	int tmp, tmp2;
	char string[MAX_NAME_LENGTH];

	/* Skip lines that start with # */
	do {
		tmp = fgetc(fp);
		if (tmp == EOF)
			return 0;

		string[0] = (char) tmp;
		/* Put the last character peeked at back */
		tmp2 = ungetc(tmp, fp);
		if (tmp2 == EOF)
			return 0;

		if (string[0] != '#')
			break;
		/* Eat up to the newline */
		tmp2 = fscanf(fp, "%[^\n]", string);
		if (tmp2 == EOF)
			return 0;
		/* Eat the newline */
		tmp = fgetc(fp);
	} while (1);
	return 1;
}

int copy_word_from_file(FILE *fp, char **new_word)
{
	char word[MAX_NAME_LENGTH];

	fscanf(fp, "%[^,]", word);
	*new_word = malloc(strlen(word) + 1);
	if (!*new_word)
		return 0;
	strcpy(*new_word, word);
	fgetc(fp);
	return 1;
}

struct plant *parse_and_create_plant(FILE *fp)
{
	struct plant *new_plant;
	char *string;

	new_plant = malloc(sizeof(*new_plant));
	if (!new_plant) {
		printf("Out of memory\n");
		return NULL;
	}
	memset(new_plant, 0, sizeof(*new_plant));

	if(!skip_comment_lines(fp))
		return NULL;

	/* Get the plant name */
	if (!copy_word_from_file(fp, &new_plant->name))
		return NULL;

	/* Get the number plants we want to harvest */
	fscanf(fp, "%u", &new_plant->num_plants_to_harvest);
	fgetc(fp);

	/* Get the number of weeks indoors */
	fscanf(fp, "%u", &new_plant->num_weeks_indoors);
	fgetc(fp);

	/* Get the number of weeks after sprouting
	 * that we need to separate the plants.
	 */
	fscanf(fp, "%u", &new_plant->num_weeks_until_indoor_separation);
	fgetc(fp);

	/* Convert the outdoor planting date into something we can understand */
	if (!copy_word_from_file(fp, &string))
		return NULL;
	if (!strptime(string, "%Y-%m-%d", &new_plant->outdoor_planting_date))
		return NULL;
	free(string);
	
	fscanf(fp, "%u", &new_plant->num_weeks_until_outdoor_separation);
	fgetc(fp);

	fscanf(fp, "%u", &new_plant->days_to_harvest);
	fgetc(fp);

	fscanf(fp, "%f", &new_plant->germination_rate);
	fgetc(fp);

	fscanf(fp, "%u", &new_plant->min_days_to_sprout);
	fgetc(fp);

	fscanf(fp, "%u", &new_plant->max_days_to_sprout);
	fgetc(fp);

	new_plant->avg_days_to_sprout = (new_plant->min_days_to_sprout +
			new_plant->max_days_to_sprout) / 2;

	fscanf(fp, "%u", &new_plant->harvest_removes_plant);
	fgetc(fp);

	return new_plant;
}

int add_days_to_date(struct tm *date, int days)
{
	date->tm_mday += days;
	/* Normalize the date */
	mktime(date);
	return 0;
}

int calculate_indoor_plant_dates(struct plant *new_plant)
{
	int ret;
	unsigned int temp;

	/* Get date to start seeds indoors */
	new_plant->seeding_date = new_plant->outdoor_planting_date;
	ret = add_days_to_date(&new_plant->seeding_date,
			-(new_plant->num_weeks_indoors)*7);
	if (ret)
		return ret;

	/* Get date to separate indoor seedlings */
	if (new_plant->num_weeks_until_indoor_separation) {
		new_plant->indoor_separation_date =
			new_plant->seeding_date;
		temp = new_plant->num_weeks_until_indoor_separation*7;
		ret = add_days_to_date(
				&new_plant->indoor_separation_date,
			       	temp);
		if (ret)
			return ret;
	}

	/* Get date to start hardening off plants (leaving them
	 * outdoors during the day, bringing them inside at night)
	 */
	new_plant->hardening_off_date =
		new_plant->outdoor_planting_date;
	ret = add_days_to_date(&new_plant->hardening_off_date, -3);
	if (ret)
		return ret;

	/* Set sprouting base date */
	new_plant->sprouting_date = new_plant->seeding_date;
	new_plant->last_chance_sprouting_date = new_plant->seeding_date;
	return 0;
}

int calculate_direct_sown_plant_dates(struct plant *new_plant)
{
	int ret;
	int temp;

	new_plant->seeding_date = new_plant->outdoor_planting_date;
	
	new_plant->sprouting_date =
		new_plant->outdoor_planting_date;
	new_plant->last_chance_sprouting_date =
		new_plant->outdoor_planting_date;
	
	if (new_plant->num_weeks_until_outdoor_separation) {
		new_plant->outdoor_separation_date =
			new_plant->outdoor_planting_date;
		temp = new_plant->num_weeks_until_outdoor_separation;
		ret = add_days_to_date(
				&new_plant->outdoor_separation_date,
				temp*7);
		if (ret)
			return ret;
	}
	return 0;
}

int calculate_plant_dates(struct plant *new_plant)
{
	int ret;

	/* Some plants need to be direct sown outdoors,
	 * rather than started under a sun lamp indoors.
	 */
	if (new_plant->num_weeks_indoors) {
		ret = calculate_indoor_plant_dates(new_plant);
	} else {
		ret = calculate_direct_sown_plant_dates(new_plant);
	}
	if (ret)
		return ret;

	ret = add_days_to_date(&new_plant->sprouting_date,
			(int) new_plant->avg_days_to_sprout);
	if (ret)
		return ret;

	ret = add_days_to_date(&new_plant->last_chance_sprouting_date,
			new_plant->max_days_to_sprout);
	if (ret)
		return ret;

	/* Harvest date is calculated from the time the seed is in the soil,
	 * either indoors or outdoors.
	 */
	new_plant->harvest_date = new_plant->seeding_date;
	ret = add_days_to_date(&new_plant->harvest_date,
			new_plant->days_to_harvest);
	if (ret)
		return ret;

	return 0;
}


/****************** By plant calendar functions ******************/

/*
 * num seeds survived = num seeds planted * germination rate
 * num seeds survived / germination rate = num seeds planted
 */
float get_num_seeds_needed(struct plant *new_plant)
{
	return ceil(new_plant->num_plants_to_harvest /
			new_plant->germination_rate);
}

void print_date(char *description, struct tm *date)
{
	char string[MAX_NAME_LENGTH];

	strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
			date);
	printf("%s: %s\n", description, string);
}

void print_indoor_plant_dates(struct plant *new_plant)
{
	char string[MAX_NAME_LENGTH];
	float num_seeds;

	num_seeds = get_num_seeds_needed(new_plant);
	strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
			&new_plant->seeding_date);
	printf("Start %i seed%s under grow lamp: %s\n",
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "",
			string);
	print_date("Expect sprouting seeds around",
			&new_plant->sprouting_date);
	print_date("Last chance for sprouting seeds",
			&new_plant->last_chance_sprouting_date);

	if (new_plant->num_weeks_until_indoor_separation)
		print_date("Separate or move to a bigger indoor pot",
				&new_plant->indoor_separation_date);

	print_date("Start hardening off seedlings",
			&new_plant->hardening_off_date);

	strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
			&new_plant->outdoor_planting_date);
	num_seeds = new_plant->num_plants_to_harvest;
	printf("Transplant %i plant%s outdoors: %s\n",
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "",
			string);
}

void print_direct_sown_plant_dates(struct plant *new_plant)
{
	char string[MAX_NAME_LENGTH];
	float num_seeds;

	num_seeds = get_num_seeds_needed(new_plant);
	strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
			&new_plant->outdoor_planting_date);
	printf("Direct sow %i seeds outdoors: %s\n",
			(int) num_seeds, string);
	print_date("Expect sprouting seeds around",
			&new_plant->sprouting_date);
	print_date("Last chance for sprouting seeds",
			&new_plant->last_chance_sprouting_date);
	
	if (new_plant->num_weeks_until_outdoor_separation) {
		strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
				&new_plant->outdoor_separation_date);
		num_seeds = new_plant->num_plants_to_harvest;
		printf("Thin to %i plant%s: %s\n",
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "",
			string);
	}
}

void print_action_dates(struct plant *new_plant)
{
	int chars_printed;
	char string[MAX_NAME_LENGTH];

	chars_printed = printf("Calendar for %s:\n", new_plant->name);
	/* Don't count the newline */
	for(; chars_printed > 1; chars_printed--)
		putchar('=');
	printf("\n");

	if (new_plant->num_weeks_indoors)
		print_indoor_plant_dates(new_plant);
	else
		print_direct_sown_plant_dates(new_plant);

	strftime(string, MAX_NAME_LENGTH, "%a, %b. %d, %Y",
			&new_plant->harvest_date);
	if (new_plant->harvest_removes_plant)
		printf("Harvest plants: %s\n", string);
	else
		printf("Start harvesting: %s\n", string);

}

/****************** By month calendar functions ******************/

/* Is this time less than that time? */
int date_is_less_than(struct tm *this,
		struct tm *that)
{
	time_t this_time;
	time_t that_time;

	/* Turn both into epoch time */
	this_time = mktime(this);
	that_time = mktime(that);

	return this_time < that_time;
}

int dates_are_equal(struct tm *this, struct tm *that)
{
	time_t this_time;
	time_t that_time;

	/* Turn both into epoch time */
	this_time = mktime(this);
	that_time = mktime(that);

	return this_time == that_time;
}

int sort_in_one_date(struct plant_date *cal_entry,
		struct date_list **head_ptr)
{
	struct date_list **next_ptr = NULL;
	struct date_list *new_date;


	new_date = malloc(sizeof(*new_date));
	if (!new_date)
		return 0;
	new_date->cal_entry = cal_entry;

	/* This is where C++ operator overloading would be great. */
	for (next_ptr = head_ptr; *next_ptr != NULL;
			next_ptr = &(*next_ptr)->next) {
		/* If the new date is less than the next item in the list,
		 * insert it before that item in the list.
		 */
		if (date_is_less_than(cal_entry->date,
					(*next_ptr)->cal_entry->date)) {
			new_date->next = *next_ptr;
			*next_ptr = new_date;
			return 1;
		}
	}
	new_date->next = NULL;
	*next_ptr = new_date;
	return 1;
}

struct plant_date *make_calendar_entry(struct tm *date,
		char *summary, char *description)
{
	struct plant_date *cal_entry;

	cal_entry = malloc(sizeof(*cal_entry));
	if (!cal_entry)
		return NULL;

	cal_entry->date = date;
	cal_entry->summary = summary;
	cal_entry->description = description;
	return cal_entry;
}

int insert_calendar_entry(struct tm *date, char *summary, char *description,
		struct date_list **head_ptr)
{
	struct plant_date *cal_entry;

	cal_entry = make_calendar_entry(date, summary, description);
	if (!cal_entry)
		return 0;

	if (!sort_in_one_date(cal_entry, head_ptr))
		return 0;
	return 1;
}

int add_sprouting_dates_to_list(struct plant *new_plant,
		struct date_list **head_ptr)
{
	char *summary;
	char *description;

	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Expect sprouting seeds around",
			new_plant->name);
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Sprouting: %s",
			new_plant->name);
	if (!insert_calendar_entry(&new_plant->sprouting_date,
				summary, description, head_ptr))
		return 0;

	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Last chance for sprouting seeds",
			new_plant->name);
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Check sprouts: %s",
			new_plant->name);
	if (!insert_calendar_entry(&new_plant->last_chance_sprouting_date,
				summary, description, head_ptr))
		return 0;
	return 1;
}

/* Organize the dates in the plant into a larger sorted date list */
int add_indoor_plant_dates_to_list(struct plant *new_plant,
		struct date_list **head_ptr, int suppress_sprouting_dates)
{
	char *summary;
	char *description;
	float num_seeds;

	if (!new_plant->num_weeks_indoors)
		return 1;

	if (!suppress_sprouting_dates) {
		if (!add_sprouting_dates_to_list(new_plant, head_ptr))
			return 0;
		return 1;
	}

	/* Starting seeds indoors */
	num_seeds = get_num_seeds_needed(new_plant);
	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Start %i seed%s under grow lamp",
			new_plant->name,
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "");
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Seed indoors: %s", new_plant->name);
	if (!insert_calendar_entry(&new_plant->seeding_date,
				summary, description, head_ptr))
		return 0;

	/* Separating seeds indoors */
	if (new_plant->num_weeks_until_indoor_separation) {
		description = malloc(sizeof(char)*MAX_NAME_LENGTH);
		if (!description)
			return 0;
		snprintf(description, MAX_NAME_LENGTH,
				"%s -- Separate or move to a bigger indoor pot",
				new_plant->name);
		summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
		if (!summary)
			return 0;
		snprintf(summary, MAX_NAME_LENGTH,
				"Separate: %s", new_plant->name);
		if (!insert_calendar_entry(&new_plant->indoor_separation_date,
					summary, description, head_ptr))
			return 0;
	}

	/* Harden off seedlings */
	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Start hardening off seedlings (leave them out during the day and bring them in at night)",
			new_plant->name);
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Harden off: %s", new_plant->name);
	if (!insert_calendar_entry(&new_plant->hardening_off_date,
			       	summary, description, head_ptr))
		return 0;

	/* Transplant outdoors */
	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	num_seeds = new_plant->num_plants_to_harvest;
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Transplant %i plant%s outdoors",
			new_plant->name,
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "");
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Transplant: %s", new_plant->name);
	if (!insert_calendar_entry(&new_plant->outdoor_planting_date,
			       	summary, description, head_ptr))
		return 0;
	return 1;
}

int add_direct_sown_plant_dates_to_list(struct plant *new_plant,
		struct date_list **head_ptr, int suppress_sprouting_dates)
{
	char *summary;
	char *description;
	float num_seeds;

	if (new_plant->num_weeks_indoors)
		return 1;

	if (!suppress_sprouting_dates) {
		if (!add_sprouting_dates_to_list(new_plant, head_ptr))
			return 0;
		return 1;
	}

	num_seeds = get_num_seeds_needed(new_plant);
	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	snprintf(description, MAX_NAME_LENGTH,
			"%s -- Direct sow %i seed%s outdoors",
			new_plant->name,
			(int) num_seeds,
			(num_seeds > 1) ? "s" : "");
	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Direct sow: %s", new_plant->name);
	if (!insert_calendar_entry(&new_plant->outdoor_planting_date,
				summary, description, head_ptr))
		return 0;

	if (new_plant->num_weeks_until_outdoor_separation) {
		description = malloc(sizeof(char)*MAX_NAME_LENGTH);
		num_seeds = new_plant->num_plants_to_harvest;
		snprintf(description, MAX_NAME_LENGTH,
				"%s -- Thin to %i plant%s",
				new_plant->name,
				(int) num_seeds,
				(num_seeds > 1) ? "s" : "");
		summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
		if (!summary)
			return 0;
		snprintf(summary, MAX_NAME_LENGTH,
				"Thin: %s", new_plant->name);
		if (!insert_calendar_entry(&new_plant->outdoor_separation_date,
					summary, description, head_ptr))
			return 0;
	}
	return 1;
}

int add_harvest_dates_to_list(struct plant *new_plant,
		struct date_list **head_ptr)
{
	char *summary;
	char *description;
	unsigned int num_plants;

	summary = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!summary)
		return 0;
	snprintf(summary, MAX_NAME_LENGTH,
			"Harvest: %s", new_plant->name);
	description = malloc(sizeof(char)*MAX_NAME_LENGTH);
	if (!description)
		return 0;
	num_plants = new_plant->num_plants_to_harvest;
	if (new_plant->harvest_removes_plant)
		snprintf(description, MAX_NAME_LENGTH,
				"%s -- Harvest %u plant%s",
				new_plant->name,
				num_plants,
				(num_plants > 1) ? "s": "");
	else
		snprintf(description, MAX_NAME_LENGTH,
				"%s -- Start harvesting",
				new_plant->name);
	if (!insert_calendar_entry(&new_plant->harvest_date,
			       	summary, description, head_ptr))
		return 0;

	return 1;
}

void print_month_and_year(struct tm *new_date)
{
	char string[MAX_NAME_LENGTH];
	int chars_printed;

	strftime(string, MAX_NAME_LENGTH,
			"\n%B %Y\n", new_date);
	chars_printed = printf(string);
	/* Don't count the newline */
	for(; chars_printed > 1; chars_printed--)
		putchar('=');
	printf("\n");
}

void make_icalendar(struct date_list *head)
{
	struct tm *new_date;
	struct tm *now;
	time_t now_time;
	struct date_list *item;
	char start_date[MAX_NAME_LENGTH];
	char end_date[MAX_NAME_LENGTH];
	char now_date[MAX_NAME_LENGTH];
	char uid[4*MAX_NAME_LENGTH];
	char ptr[MAX_NAME_LENGTH];

	if (!head)
		return;
	
	/* Standard ical stuff */ 
	printf("BEGIN:VCALENDAR\r\n");
	printf("VERSION:2.0\r\n");
	printf("PRODID:-//Sarah Sharp//Garden Calendar Tool v0.1//EN\r\n");
	/* Following RFC at http://www.ietf.org/rfc/rfc2445.txt */

	for (item = head; item != NULL; item = item->next) {
		new_date = item->cal_entry->date;
		printf("BEGIN:VEVENT\r\n");

		/* What to use as a unique ID?  Must be "globally unique
		 * across icalendars.  Seconds since 1970 + hash of
		 * name?  No requirement that plant names be unique
		 * across one garden.
		 */
		time(&now_time);
		now = localtime(&now_time);
		strftime(uid, MAX_NAME_LENGTH, "%s", now);
		snprintf(ptr, MAX_NAME_LENGTH, "%p@SSGCT", item);
		strncat(uid, ptr, 4*MAX_NAME_LENGTH - 1);
		printf("UID:%s\r\n", uid);

		strftime(start_date, MAX_NAME_LENGTH, "%Y%m%d",
				new_date);

		/* Make the calendar entry last all day for now */
		new_date->tm_mday += 1;
		mktime(new_date);
		strftime(end_date, MAX_NAME_LENGTH, "%Y%m%d",
				new_date);

		strftime(now_date, MAX_NAME_LENGTH, "%Y%m%dT%H%M%S",
				now);
		printf("DTSTAMP:%s\r\n", now_date);
		printf("DTSTART;VALUE=DATE:%s\r\n", start_date);
		printf("DTEND;VALUE=DATE:%s\r\n", end_date);
		printf("SUMMARY:%s\r\n", item->cal_entry->summary);
		printf("DESCRIPTION:%s\r\n", item->cal_entry->description);
		new_date->tm_mday -= 1;
		mktime(new_date);
		printf("END:VEVENT\r\n");
	}
	printf("END:VCALENDAR\r\n");
}

void print_by_month_calendar(struct date_list *head)
{
	int cur_month, cur_year;
	struct tm *old_date = NULL;
	struct tm *new_date;
	struct date_list *item;
	char string[MAX_NAME_LENGTH];

	if (!head)
		return;

	new_date = head->cal_entry->date;
	print_month_and_year(new_date);
	cur_month = new_date->tm_mon;
	cur_year = new_date->tm_year;

	for (item = head; item != NULL; item = item->next) {
		new_date = item->cal_entry->date;
		if (cur_month != new_date->tm_mon ||
				cur_year != new_date->tm_year) {
			printf("\n");
			print_month_and_year(new_date);
			cur_month = new_date->tm_mon;
			cur_year = new_date->tm_year;
		}
		strftime(string, MAX_NAME_LENGTH, "%e (%a)",
				new_date);
		if (old_date == NULL ||
				!dates_are_equal(old_date, new_date))
			printf("\n   %s: %s\n", string,
					item->cal_entry->description);
		else
			printf("             %s\n",
					item->cal_entry->description);
		old_date = new_date;
	}
}

#define	BY_PLANT	(1 << 0)
#define	BY_MONTH	(1 << 1)
#define	BY_HARVEST	(1 << 2)
#define	BY_SPROUTING	(1 << 3)

int main (int argc, char *argv[])
{
	FILE *fp;
	struct plant *new_plant;
	struct date_list *sprouting_list_head = NULL;
	struct date_list *action_list_head = NULL;
	struct date_list *harvest_list_head = NULL;
	unsigned int chars_printed;
	unsigned int calendar_bitmask = 0;
	int i;
	int use_ical = 0;

	if (argc < 2) {
		printf("Help: plant <file> [output type] [options]...\n");
		printf("Where [output type] can be:\n");
		printf("  p for a by-plant calendar\n");
		printf("  m for a by-month calendar\n");
		printf("  h for a harvest calendar\n");
		printf("  s for a seed sprouting calendar\n");
		printf("Where [options] can be:\n");
		printf("  i to use ical format instead of plain text\n");
		return -1;
	}
	fp = fopen(argv[1], "r");
	if (!fp) {
		printf("Bad file.\n");
		return -1;
	}

	for (i = 2; i < (2+4) && i < argc; i++) {
		if (!strcmp(argv[i], "p") ||
				!strcmp(argv[i], "-p"))
			calendar_bitmask |= BY_PLANT;
		if (!strcmp(argv[i], "m") ||
				!strcmp(argv[i], "-m"))
			calendar_bitmask |= BY_MONTH;
		if (!strcmp(argv[i], "h") ||
				!strcmp(argv[i], "-h"))
			calendar_bitmask |= BY_HARVEST;
		if (!strcmp(argv[i], "s") ||
				!strcmp(argv[i], "-s"))
			calendar_bitmask |= BY_SPROUTING;
		if (!strcmp(argv[i], "i") ||
				!strcmp(argv[i], "-i"))
			use_ical = 1;
	}

	while (1) {
		new_plant = parse_and_create_plant(fp);
		if (!new_plant)
			break;
		calculate_plant_dates(new_plant);
		if (calendar_bitmask & BY_PLANT) {
			printf("\n");
			print_action_dates(new_plant);
			printf("\n");
		}
		if (calendar_bitmask & BY_MONTH) {
			if (!add_indoor_plant_dates_to_list(new_plant,
					&action_list_head, 1))
				return -1;
			if (!add_direct_sown_plant_dates_to_list(new_plant,
					&action_list_head, 1))
				return -1;
		}
		if (calendar_bitmask & BY_SPROUTING) {
			if (!add_indoor_plant_dates_to_list(new_plant,
					&sprouting_list_head, 0))
				return -1;
			if (!add_direct_sown_plant_dates_to_list(new_plant,
					&sprouting_list_head, 0))
				return -1;
		}
		if (calendar_bitmask & BY_HARVEST) {
			if (!add_harvest_dates_to_list(new_plant,
					&harvest_list_head))
				return -1;
		}
	}

	if (calendar_bitmask & BY_MONTH) {
		if (use_ical)
			make_icalendar(action_list_head);
		else {
			chars_printed = printf("\n\nGarden Action Items Calendar\n");
			for(; chars_printed > 3; chars_printed--)
				putchar('*');
			printf("\n");
			print_by_month_calendar(action_list_head);
		}
	}

	if (calendar_bitmask & BY_SPROUTING) {
		if (use_ical)
			make_icalendar(sprouting_list_head);
		else {
			chars_printed = printf("\n\nSeed Sprouting Calendar\n");
			for(; chars_printed > 3; chars_printed--)
				putchar('*');
			printf("\n");
			print_by_month_calendar(sprouting_list_head);
		}
	}

	if (calendar_bitmask & BY_HARVEST) {
		if (use_ical)
			make_icalendar(harvest_list_head);
		else {
			chars_printed = printf("\n\nHarvest Calendar\n");
			for(; chars_printed > 3; chars_printed--)
				putchar('*');
			printf("\n");
			print_by_month_calendar(harvest_list_head);
		}
	}

	return 0;
}

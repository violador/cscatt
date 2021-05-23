#include "c_lib.h"
#include "utils.h"
#include "string.h"

#define STRING_DEFAULT_LENGTH 1024

struct string
{
	char *array, **token;
	size_t length, max_length, counter;
};

void string_increase_storage(string *s)
{
	s->max_length += STRING_DEFAULT_LENGTH + 1;

	s->array = realloc(s->array, sizeof(char)*s->max_length);

	ASSERT(s->array != NULL)
}

string *string_alloc()
{
	string *s = allocate(1, sizeof(string), true);

	string_increase_storage(s);

	return s;
}

void string_free(string *s)
{
	if (s->token != NULL) free(s->token);

	free(s->array);

	free(s);
}

void string_set(string *s, const char text[])
{
	ASSERT(text != NULL)

	const size_t length = strlen(text);

	while (s->max_length < length)
		string_increase_storage(s);

	for (size_t n = 0; n < length; ++n)
		s->array[n] = text[n];

	s->array[length] = '\0';

	s->length = length;
}

size_t string_length(const string *s)
{
	return s->length;
}

int string_max_length(const string *s)
{
	return s->max_length;
}

void string_swap(string *a, string *b)
{
	char *b_array = b->array;
	char **b_token = b->token;
	const size_t b_length = b->length;
	const size_t b_counter = b->counter;
	const size_t b_max_length = b->max_length;

	b->array = a->array;
	b->token = a->token;
	b->length = a->length;
	b->counter = a->counter;
	b->max_length = a->max_length;

	a->array = b_array;
	a->token = b_token;
	a->length = b_length;
	a->counter = b_counter;
	a->max_length = b_max_length;
}

string *string_substring(const string *s, const size_t start, const size_t end)
{
	ASSERT(end >= start)
	ASSERT(end < s->length)

	string *new_s = string_alloc();

	const size_t length = end - start + 1;

	while (new_s->max_length < length)
		string_increase_storage(new_s);

	for (size_t n = 0; n < length; ++n)
		new_s->array[n] = s->array[start + n];

	new_s->array[length] = '\0';

	new_s->length = length;

	return new_s;
}

void string_crop(string *s, const size_t start, const size_t end)
{
	if (start == 0)
	{
		s->length = end + 1;
		s->array[end + 1] = '\0';
	}

	else if (start > 0)
	{
		string *new_s = string_substring(s, start, end);

		string_swap(s, new_s);

		string_free(new_s);
	}
}

string *string_copy(const string *s)
{
	return string_substring(s, 0, s->length - 1);
}

void string_append(string *s, const char text[])
{
	ASSERT(text != NULL)

	const size_t length = strlen(text);

	while ((s->max_length - s->length) < length)
		string_increase_storage(s);

	for (size_t n = 0; n < length; ++n)
		s->array[s->length + n] = text[n];

	s->length += length;

	s->array[s->length] = '\0';
}

void string_print(const string *s, FILE *stream, const bool end_line)
{
	ASSERT(stream != NULL)

	if (end_line)
		fprintf(stream, "%s\n", s->array);
	else
		fprintf(stream, "%s", s->array);
}

int string_count(const string *s, const char pattern[])
{
	ASSERT(pattern != NULL)

	const size_t length = strlen(pattern);

	size_t counter = 0;

	for (size_t n = 0; n < s->length; n += length)
		if (strncmp(s->array + n, pattern, length) == 0) ++counter;

	return counter;
}

void string_insert(string *s, const size_t n, const char text[])
{
	ASSERT(text != NULL)

	string *old_s = string_copy(s);

	string_crop(s, 0, n - 1);
	string_append(s, text);

	string_crop(old_s, n, old_s->length - 1);
	string_concatenate(s, old_s);

	string_free(old_s);
}

void string_remove(string *s, const size_t start, const size_t end)
{
	ASSERT(end >= start)
	ASSERT(end < (s->length - 1))

	string *old_s = string_copy(s);

	string_crop(s, 0, start - 1);
	string_crop(old_s, end + 1, old_s->length - 1);

	string_concatenate(s, old_s);

	string_free(old_s);
}

void string_replace_all(string *s, const char pattern[], const char insert[])
{
	ASSERT(insert != NULL)
	ASSERT(pattern != NULL)

	const size_t new_length = strlen(insert);
	const size_t old_length = strlen(pattern);

	if (new_length == old_length)
	{
		for (size_t n = 0; n < s->length; n += old_length)
		{
			if (strncmp(s->array + n, pattern, old_length) == 0)
				strncpy(s->array + n, insert, old_length);
		}
	}
	else
	{
		for (size_t n = 0; n < s->length; n += old_length)
		{
			if (strncmp(s->array + n, pattern, old_length) == 0)
			{
				string *old_s = string_copy(s);

				string_crop(s, 0, n - 1);
				string_append(s, insert);

				string_crop(old_s, n + 1, old_s->length - 1);
				string_concatenate(s, old_s);

				string_free(old_s);

				n += new_length;
			}
		}
	}
}

void string_replace_at(string *s,
                       const char pattern[], const char insert[], const size_t n)
{
	ASSERT(insert != NULL)
	ASSERT(pattern != NULL)
	ASSERT(n < s->length)
	ASSERT(strlen(pattern) == strlen(insert))

	const size_t length = strlen(pattern);

	if (strncmp(s->array + n, pattern, length) == 0)
		strncpy(s->array + n, insert, length);
}

void string_right_trim(string *s)
{
	size_t counter = 0;

	for (size_t n = (s->length - 1); n < s->length; --n)
	{
		if (s->array[n] == ' ')
			++counter;
		else
			break;
	}

	s->length -= counter;

	s->array[s->length] = '\0';
}

void string_left_trim(string *s)
{
	size_t counter = 0;

	for (size_t n = 0; n < s->length; ++n)
	{
		if (s->array[n] == ' ')
			++counter;
		else
			break;
	}

	s->length -= counter;

	for (size_t n = 0; n <= s->length; ++n)
		s->array[n] = s->array[counter + n];
}

void string_trim(string *s)
{
	string_right_trim(s);
	string_left_trim(s);
}

void string_reset(string *s)
{
	s->length = 0;
	s->counter = 0;
	s->array[0] = '\0';

	if (s->token != NULL)
	{
		free(s->token);
		s->token = NULL;
	}
}

char string_at(string *s, const size_t n)
{
	ASSERT(n < s->length)

	return s->array[n];
}

void string_read_file(string *s, const char filename[])
{
	ASSERT(filename != NULL)

	FILE *stream = fopen(filename, "r");

	if (stream == NULL)
	{
		PRINT_ERROR("unable to open %s\n", filename)
		exit(EXIT_FAILURE);
	}

	fseek(stream, 0, SEEK_END);

	const size_t length = ftell(stream);

	while (s->max_length < length)
		string_increase_storage(s);

	fseek(stream, 0, SEEK_SET);

	const size_t info = fread(s->array, sizeof(char), length, stream);
	ASSERT(info == length)

	s->array[length] = '\0';

	s->length = length;

	fclose(stream);
}

void string_set_lower(string *s)
{
	for (size_t n = 0; n < s->length; ++n)
		s->array[n] = tolower(s->array[n]);
}

string *string_file_line(const string *s, const size_t n)
{
	size_t counter = 0, start = 0, end = 0;

	for (size_t m = 0; m < s->length; ++m)
	{
		if (s->array[m] == '\n')
		{
			++counter;
			end = m;

			if (counter == n) break;

			start = m + 1;
		}
	}

	string *line = string_alloc();

	if (counter == 0) return line;

	const size_t length = (s->array + end) - (s->array + start) + 1;

	while (line->max_length < length)
		string_increase_storage(line);

	strncpy(line->array, s->array + start, length);

	line->length = length;

	return line;
}

void string_set_upper(string *s)
{
	for (size_t n = 0; n < s->length; ++n)
		s->array[n] = toupper(s->array[n]);
}

bool string_compare(const string *a, const string *b)
{
	if (a->length == b->length)
		return (strcmp(a->array, b->array) == 0);
	else
		return false;
}

int string_find_first(const string *s, const char pattern[])
{
	ASSERT(pattern != NULL)

	const size_t length = strlen(pattern);

	for (size_t n = 0; n < s->length; n += length)
		if (strncmp(s->array + n, pattern, length) == 0) return (int) n;

	return -1;
}

int string_find_from(const string *s, const char pattern[], const size_t start)
{
	ASSERT(pattern != NULL)
	ASSERT(start < s->length)

	const size_t length = strlen(pattern);

	for (size_t n = start; n < s->length; n += length)
		if (strncmp(s->array + n, pattern, length) == 0) return (int) n;

	return -1;
}

void string_concatenate(string *a, const string *b)
{
	string_append(a, b->array);
}

char *string_as_array(const string *s)
{
	char *copy = allocate(s->length + 1, sizeof(char), false);

	for (size_t n = 0; n < s->length; ++n)
		copy[n] = s->array[n];

	copy[s->length] = '\0';

	return copy;
}

void string_tokenize(string *s, const char delim[])
{
	ASSERT(delim != NULL)

	const size_t length = strlen(delim);

	s->counter = 0;

	size_t start = 0;
	for (size_t n = 0; n < s->length; n += length)
	{
		if (strncmp(s->array + n, delim, length) == 0)
		{
			strncpy(s->array + n, "\0", length);
			++s->counter;

			s->token = realloc(s->token, sizeof(char *)*s->counter);
			s->token[s->counter - 1] = s->array + start;

			start = n + length;
		}
	}
}

int string_token_count(const string *s)
{
	return s->counter;
}

void string_token_print(const string *s,
                        const size_t n, FILE *stream, const bool end_line)
{
	ASSERT(stream != NULL)
	ASSERT(s->token != NULL)
	ASSERT(n < s->counter)

	if (end_line)
		fprintf(stream, "%s\n", s->token[n]);
	else
		fprintf(stream, "%s", s->token[n]);
}

void string_token_print_all(const string *s, FILE *stream, const bool end_line)
{
	for (size_t n = 0; n < s->counter; ++n)
		string_token_print(s, n, stream, end_line);
}

void string_token_trim(string *s, const size_t n)
{
	ASSERT(s->token != NULL)
	ASSERT(n < s->counter)

	size_t m = 0;
	while ((s->token[n][m] == ' ') || (s->token[n][m] == '\t'))
	{
		++m;
		s->token[n] = s->token[n] + 1;
	}
}

void string_token_trim_all(string *s)
{
	for (size_t n = 0; n < s->counter; ++n)
		string_token_trim(s, n);
}

void string_token_replace(string *s, const size_t n,
                          const char pattern[], const char insert[])
{
	ASSERT(insert != NULL)
	ASSERT(pattern != NULL)
	ASSERT(s->token != NULL)
	ASSERT(n < s->counter)
	ASSERT(strlen(pattern) == strlen(insert))

	const size_t length = strlen(pattern);

	for (size_t m = 0; m < strlen(s->token[n]); m += length)
	{
		if (strncmp(s->token[n] + m, pattern, length) == 0)
			strncpy(s->token[n] + m, insert, length);
	}
}

void string_token_replace_all(string *s,
                              const char pattern[], const char insert[])
{
	for (size_t n = 0; n < s->counter; ++n)
		string_token_replace(s, n, pattern, insert);
}

size_t string_token_length(string *s, const size_t n)
{
	ASSERT(s->token != NULL)
	ASSERT(n < s->counter)

	return strlen(s->token[n]);
}

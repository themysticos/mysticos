// api.h - API для терминальных программ
#ifndef API_H
#define API_H

extern void api_console_print(const char* str);
extern char* api_user_input(char* buffer, int max_len);
extern void api_set_cursor_pos(int x, int y);
extern void api_get_cursor_pos(int* x, int* y);

#endif
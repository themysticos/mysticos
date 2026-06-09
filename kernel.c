// kernel.c - Ядро MysticOS с ФС и пользователями
#include "api.h"

#define MAX_CMD_LEN 256
#define MAX_FILES 32
#define MAX_USERS 16
#define MAX_FILENAME 32
#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define FILE_CONTENT_SIZE 1024
#define NULL ((void*)0)

// ===== СТРУКТУРЫ ДАННЫХ =====
typedef struct {
    char name[MAX_FILENAME];
    char content[FILE_CONTENT_SIZE];
    int size;
    int used;
} File;

typedef struct {
    char name[32];
    void (*func)(char*);
    char* help;
} Command;

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int is_admin;
} User;

// ===== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ =====
static File filesystem[MAX_FILES];
static User users[MAX_USERS];
static int user_count = 0;
static int current_user = -1;  // -1 = не залогинен
static char current_username[MAX_USERNAME] = "guest";

// ===== ПРОТОТИПЫ =====
static void clear_screen(void);
static void kernel_print(const char* str);
static void execute_command(char* cmd_line);
static int mystrcmp(const char* s1, const char* s2);
static int mystrlen(const char* str);
static void mystrcpy(char* dest, const char* src);
static void kernel_panic(const char* msg);
static void init_filesystem(void);
static void init_users(void);
static void show_prompt(void);
static int login_user(const char* username, const char* password);
static void create_file(const char* name, const char* content);
static char* read_file(const char* name);
static void delete_file(const char* name);
static void list_files(void);

// ===== ИНИЦИАЛИЗАЦИЯ =====
static void init_filesystem(void) {
    for(int i = 0; i < MAX_FILES; i++) {
        filesystem[i].used = 0;
        filesystem[i].size = 0;
        filesystem[i].name[0] = 0;
    }
}

static void init_users(void) {
    // Создаём root и guest
    mystrcpy(users[0].username, "root");
    mystrcpy(users[0].password, "12345");
    users[0].is_admin = 1;
    
    mystrcpy(users[1].username, "guest");
    mystrcpy(users[1].password, "");
    users[1].is_admin = 0;
    
    user_count = 2;
    
    // Создаём приветственный файл
    create_file("welcome.txt", 
        "Welcome to MysticOS!\n"
        "Type 'help' for commands.\n"
        "Login with 'login root 12345'\n");
    
    create_file("readme.txt",
        "MysticOS v1.0 Filesystem\n"
        "Commands: create, read, delete, files\n");
}

// ===== ФАЙЛОВЫЕ ОПЕРАЦИИ =====
static void create_file(const char* name, const char* content) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(!filesystem[i].used) {
            mystrcpy(filesystem[i].name, name);
            mystrcpy(filesystem[i].content, content);
            filesystem[i].size = mystrlen(content);
            filesystem[i].used = 1;
            return;
        }
    }
    kernel_print("Error: Filesystem full!\n");
}

static char* read_file(const char* name) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(filesystem[i].used && mystrcmp(filesystem[i].name, name) == 0) {
            return filesystem[i].content;
        }
    }
    return NULL;
}

static void delete_file(const char* name) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(filesystem[i].used && mystrcmp(filesystem[i].name, name) == 0) {
            filesystem[i].used = 0;
            filesystem[i].name[0] = 0;
            filesystem[i].size = 0;
            kernel_print("File deleted.\n");
            return;
        }
    }
    kernel_print("File not found.\n");
}

static void list_files(void) {
    int count = 0;
    kernel_print("Files:\n");
    for(int i = 0; i < MAX_FILES; i++) {
        if(filesystem[i].used) {
            kernel_print("  ");
            kernel_print(filesystem[i].name);
            kernel_print(" (");
            // Простой вывод размера
            char size_str[10];
            int sz = filesystem[i].size;
            int pos = 0;
            if(sz == 0) {
                size_str[0] = '0';
                size_str[1] = 0;
            } else {
                while(sz > 0) {
                    size_str[pos++] = '0' + (sz % 10);
                    sz /= 10;
                }
                size_str[pos] = 0;
                // Реверс
                for(int j = 0; j < pos/2; j++) {
                    char tmp = size_str[j];
                    size_str[j] = size_str[pos-1-j];
                    size_str[pos-1-j] = tmp;
                }
            }
            kernel_print(size_str);
            kernel_print(" bytes)\n");
            count++;
        }
    }
    if(count == 0) kernel_print("  (empty)\n");
}

// ===== ПОЛЬЗОВАТЕЛИ =====
static int login_user(const char* username, const char* password) {
    for(int i = 0; i < user_count; i++) {
        if(mystrcmp(users[i].username, username) == 0) {
            if(mystrcmp(users[i].password, password) == 0) {
                current_user = i;
                mystrcpy(current_username, username);
                return 1;
            }
        }
    }
    return 0;
}

static void show_prompt(void) {
    kernel_print(current_username);
    kernel_print("@mystic> ");
}

// ===== КОМАНДЫ =====
static void cmd_help(char* args) {
    kernel_print("\n=== MysticOS Commands ===\n");
    kernel_print("help            - Show help\n");
    kernel_print("clear           - Clear screen\n");
    kernel_print("echo [msg]      - Print message\n");
    kernel_print("ver             - OS version\n");
    kernel_print("info            - System info\n");
    kernel_print("login [u] [p]   - Login\n");
    kernel_print("logout          - Logout\n");
    kernel_print("whoami          - Show user\n");
    kernel_print("files           - List files\n");
    kernel_print("create [n] [c]  - Create file\n");
    kernel_print("read [n]        - Read file\n");
    kernel_print("delete [n]      - Delete file\n");
    kernel_print("write [n] [c]   - Write to file\n");
    kernel_print("reboot          - Reboot\n");
    kernel_print("===========================\n");
}

static void cmd_clear(char* args) {
    clear_screen();
    api_set_cursor_pos(0, 0);
}

static void cmd_echo(char* args) {
    if(args && args[0]) kernel_print(args);
    kernel_print("\n");
}

static void cmd_ver(char* args) {
    kernel_print("MysticOS v1.1.0\nBuild: " __DATE__ " " __TIME__ "\n");
}

static void cmd_info(char* args) {
    kernel_print("=== System Info ===\n");
    kernel_print("OS: MysticOS v1.1\n");
    kernel_print("Kernel: Monolithic x86\n");
    kernel_print("FS: RAM-based\n");
    kernel_print("Users: "); 
    char buf[4];
    buf[0] = '0' + user_count;
    buf[1] = 0;
    kernel_print(buf);
    kernel_print("\n");
    kernel_print("Files: ");
    int fc = 0;
    for(int i = 0; i < MAX_FILES; i++) if(filesystem[i].used) fc++;
    buf[0] = '0' + fc;
    kernel_print(buf);
    kernel_print("\n==================\n");
}

static void cmd_login(char* args) {
    if(!args) {
        kernel_print("Usage: login <username> <password>\n");
        return;
    }
    
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int i = 0, j = 0;
    
    while(args[i] && args[i] != ' ' && j < MAX_USERNAME-1) username[j++] = args[i++];
    username[j] = 0;
    
    if(args[i] == ' ') i++;
    j = 0;
    while(args[i] && args[i] != ' ' && j < MAX_PASSWORD-1) password[j++] = args[i++];
    password[j] = 0;
    
    if(login_user(username, password)) {
        kernel_print("Login successful! Welcome, ");
        kernel_print(username);
        kernel_print("!\n");
    } else {
        kernel_print("Login failed!\n");
    }
}

static void cmd_logout(char* args) {
    current_user = 1;  // guest
    mystrcpy(current_username, "guest");
    kernel_print("Logged out.\n");
}

static void cmd_whoami(char* args) {
    kernel_print("User: ");
    kernel_print(current_username);
    if(current_user >= 0 && users[current_user].is_admin) {
        kernel_print(" (admin)");
    }
    kernel_print("\n");
}

static void cmd_files(char* args) {
    list_files();
}

static void cmd_create(char* args) {
    if(!args) {
        kernel_print("Usage: create <filename> <content>\n");
        return;
    }
    
    char filename[MAX_FILENAME];
    int i = 0, j = 0;
    
    while(args[i] && args[i] != ' ' && j < MAX_FILENAME-1) filename[j++] = args[i++];
    filename[j] = 0;
    
    char* content = "";
    if(args[i] == ' ') {
        content = &args[i+1];
        while(content[0] == ' ') content++;
    }
    
    create_file(filename, content);
    kernel_print("File '");
    kernel_print(filename);
    kernel_print("' created.\n");
}

static void cmd_read(char* args) {
    if(!args) {
        kernel_print("Usage: read <filename>\n");
        return;
    }
    
    char filename[MAX_FILENAME];
    int i = 0;
    while(args[i] && args[i] != ' ' && i < MAX_FILENAME-1) {
        filename[i] = args[i];
        i++;
    }
    filename[i] = 0;
    
    char* content = read_file(filename);
    if(content) {
        kernel_print("=== ");
        kernel_print(filename);
        kernel_print(" ===\n");
        kernel_print(content);
        kernel_print("\n=== EOF ===\n");
    } else {
        kernel_print("File not found.\n");
    }
}

static void cmd_delete(char* args) {
    if(!args) {
        kernel_print("Usage: delete <filename>\n");
        return;
    }
    
    char filename[MAX_FILENAME];
    int i = 0;
    while(args[i] && args[i] != ' ' && i < MAX_FILENAME-1) {
        filename[i] = args[i];
        i++;
    }
    filename[i] = 0;
    
    delete_file(filename);
}

static void cmd_write(char* args) {
    if(!args) {
        kernel_print("Usage: write <filename> <content>\n");
        return;
    }
    
    char filename[MAX_FILENAME];
    int i = 0, j = 0;
    
    while(args[i] && args[i] != ' ' && j < MAX_FILENAME-1) filename[j++] = args[i++];
    filename[j] = 0;
    
    char* content = "";
    if(args[i] == ' ') {
        content = &args[i+1];
        while(content[0] == ' ') content++;
    }
    
    // Удаляем старый и создаём новый
    delete_file(filename);
    create_file(filename, content);
    kernel_print("File '");
    kernel_print(filename);
    kernel_print("' updated.\n");
}

static void cmd_reboot(char* args) {
    kernel_print("Rebooting...\n");
    for(volatile int i = 0; i < 1000000; i++);
    __asm__ volatile(
        "movb $0xFE, %%al\n"
        "outb %%al, $0x64\n"
        "cli\n"
        "hlt\n"
        : : : "al"
    );
}

Command commands[] = {
    {"help",    cmd_help,    "Show help"},
    {"clear",   cmd_clear,   "Clear screen"},
    {"echo",    cmd_echo,    "Print message"},
    {"ver",     cmd_ver,     "OS version"},
    {"info",    cmd_info,    "System info"},
    {"login",   cmd_login,   "Login to system"},
    {"logout",  cmd_logout,  "Logout"},
    {"whoami",  cmd_whoami,  "Show current user"},
    {"files",   cmd_files,   "List files"},
    {"create",  cmd_create,  "Create file"},
    {"read",    cmd_read,    "Read file"},
    {"delete",  cmd_delete,  "Delete file"},
    {"write",   cmd_write,   "Write to file"},
    {"reboot",  cmd_reboot,  "Reboot system"},
    {"",        NULL,        NULL}
};

// ===== СИСТЕМНЫЕ ФУНКЦИИ =====
static int mystrcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int mystrlen(const char* str) {
    int len = 0;
    while(str[len]) len++;
    return len;
}

static void mystrcpy(char* dest, const char* src) {
    int i = 0;
    while(src[i]) { dest[i] = src[i]; i++; }
    dest[i] = 0;
}

static void clear_screen(void) {
    char* vidmem = (char*)0xB8000;
    for(int i = 0; i < 80*25*2; i += 2) {
        vidmem[i] = ' ';
        vidmem[i+1] = 0x0F;
    }
}

static void kernel_print(const char* str) {
    api_console_print(str);
}

static void kernel_panic(const char* msg) {
    clear_screen();
    api_set_cursor_pos(0, 0);
    char* vidmem = (char*)0xB8000;
    for(int i = 0; i < 80*25*2; i += 2) {
        vidmem[i] = ' ';
        vidmem[i+1] = 0x4F;
    }
    api_set_cursor_pos(0, 0);
    kernel_print("!!! KERNEL PANIC !!!\n\n");
    kernel_print(msg);
    kernel_print("\n\nSystem halted.\n");
    __asm__ volatile("cli; hlt");
    while(1);
}

static void execute_command(char* cmd_line) {
    char cmd[32];
    char* args = NULL;
    for(int i = 0; i < 32; i++) cmd[i] = 0;
    
    int i = 0;
    while(cmd_line[i] && cmd_line[i] != ' ' && i < 31) cmd[i++] = cmd_line[i];
    
    if(cmd_line[i] == ' ') {
        args = &cmd_line[i+1];
        while(args[0] == ' ') args++;
    }
    
    if(cmd[0] == 0) return;
    
    for(int j = 0; commands[j].func != NULL; j++) {
        if(mystrcmp(cmd, commands[j].name) == 0) {
            commands[j].func(args);
            return;
        }
    }
    
    kernel_print("Unknown command: '");
    kernel_print(cmd);
    kernel_print("'\nType 'help' for commands.\n");
}

// ===== MAIN =====
void kernel_main(unsigned long magic, unsigned long addr) {
    init_filesystem();
    init_users();
    current_user = 1;  // guest по умолчанию
    
    clear_screen();
    api_set_cursor_pos(0, 0);
    
    kernel_print("╔═══════════════════════════╗\n");
    kernel_print("║    MysticOS v1.1.0       ║\n");
    kernel_print("║  Type 'help' for help    ║\n");
    kernel_print("╚═══════════════════════════╝\n\n");
    
    char cmd_buffer[MAX_CMD_LEN];
    
    while(1) {
        for(int i = 0; i < MAX_CMD_LEN; i++) cmd_buffer[i] = 0;
        
        show_prompt();
        api_user_input(cmd_buffer, MAX_CMD_LEN);
        
        if(cmd_buffer[0] != 0) {
            execute_command(cmd_buffer);
            kernel_print("\n");
        }
    }
}
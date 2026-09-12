// Bare-Metal VGA Driver, PIT Timer, Mode 13h Graphics, RAM VFS, Flux Engine, Interactive Mouse & Editor
#define VGA_TEXT_ADDR 0xB8000
#define VGA_GFX_ADDR  0xA0000
#define BUF_SIZE 2000
#define MAX_CMD_LEN 256
#define MAX_VARS 32
#define MAX_FILES 16
#define FILE_SIZE 512

typedef unsigned char uint8_t;
typedef signed char   int8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

volatile uint16_t* vga_text = (uint16_t*)VGA_TEXT_ADDR;
volatile uint8_t* vga_gfx  = (uint8_t*)VGA_GFX_ADDR;
int vga_index = 0;

volatile uint32_t timer_ticks = 0;

uint32_t heap_curr = 0x200000;
void* kmalloc(uint32_t size) {
    void* ptr = (void*)heap_curr;
    heap_curr += size;
    return ptr;
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

int atoi(const char* str) {
    int res = 0;
    while (*str == ' ') str++;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

void itoa(int num, char* str) {
    int i = 0;
    bool is_neg = false;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if (num < 0) { is_neg = true; num = -num; }
    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }
    if (is_neg) str[i++] = '-';
    str[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char temp = str[j]; str[j] = str[i - j - 1]; str[i - j - 1] = temp;
    }
}

uint16_t make_vgaentry(char c, uint8_t color) {
    return (uint16_t) c | (uint16_t) color << 8;
}

void print_char(char c, uint8_t color = 0x0F) {
    if (c == '\n') {
        vga_index += 80 - (vga_index % 80);
        return;
    }
    if (c == '\b') {
        if (vga_index > 0) {
            vga_index--;
            vga_text[vga_index] = make_vgaentry(' ', color);
        }
        return;
    }
    vga_text[vga_index++] = make_vgaentry(c, color);
}

void print_str(const char* str, uint8_t color = 0x0F) {
    for (int i = 0; str[i] != '\0'; ++i) print_char(str[i], color);
}

void clear_screen() {
    for (int i = 0; i < BUF_SIZE; i++) vga_text[i] = make_vgaentry(' ', 0x0F);
    vga_index = 0;
}

// --- MODE 13H GRAPHICS PRIMITIVES ---
void draw_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < 320 && y >= 0 && y < 200) {
        vga_gfx[y * 320 + x] = color;
    }
}

void draw_rect(int x, int y, int w, int h, uint8_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            draw_pixel(j, i, color);
        }
    }
}

void demo_graphics() {
    for (int i = 0; i < 320 * 200; i++) vga_gfx[i] = 0x00;
    draw_rect(20, 20, 100, 60, 0x0A);
    draw_rect(140, 50, 80, 80, 0x0C);
    draw_rect(80, 110, 150, 40, 0x01);
}

// --- PS/2 MOUSE DRIVER ---
int mouse_x = 160;
int mouse_y = 100;
uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

void mouse_write(uint8_t val) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, val);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    mouse_wait(1);
    outb(0x64, 0xA8);
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    uint8_t status = inb(0x60) | 2;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
}

void draw_mouse_cursor(int x, int y, uint8_t color) {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void poll_mouse() {
    uint8_t status = inb(0x64);
    if ((status & 1) && (status & 32)) {
        int8_t b = inb(0x60);
        mouse_byte[mouse_cycle++] = b;
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            mouse_x += mouse_byte[1];
            mouse_y -= mouse_byte[2];

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x > 315) mouse_x = 315;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y > 195) mouse_y = 195;

            // Render cursor directly on mouse packet update
            draw_mouse_cursor(mouse_x, mouse_y, (mouse_byte[0] & 1) ? 0x0C : 0x0F);
        }
    }
}

// --- RAM VFS ---
struct VFile {
    char name[32];
    char content[FILE_SIZE];
    bool used;
};

VFile vfs_table[MAX_FILES];

void vfs_init() {
    for (int i = 0; i < MAX_FILES; i++) vfs_table[i].used = false;
    vfs_table[0].used = true;
    const char* default_name = "demo.flux";
    for (int i = 0; default_name[i]; i++) vfs_table[0].name[i] = default_name[i];
    vfs_table[0].name[9] = '\0';
    
    const char* default_code = "a=100\nif a > 50 print 999\nrect 10 10 50 50 12\n";
    for (int i = 0; default_code[i]; i++) vfs_table[0].content[i] = default_code[i];
}

void vfs_list() {
    print_str("--- RAM File System ---\n", 0x0B);
    for (int i = 0; i < MAX_FILES; i++) {
        if (vfs_table[i].used) {
            print_str("  ", 0x0F);
            print_str(vfs_table[i].name, 0x0A);
            print_char('\n');
        }
    }
}

VFile* vfs_get(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (vfs_table[i].used && strcmp(vfs_table[i].name, name) == 0) {
            return &vfs_table[i];
        }
    }
    return 0;
}

void vfs_touch(const char* name) {
    if (vfs_get(name)) {
        print_str("File already exists.\n", 0x0C);
        return;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!vfs_table[i].used) {
            vfs_table[i].used = true;
            int j = 0;
            while (name[j] && j < 31) { vfs_table[i].name[j] = name[j]; j++; }
            vfs_table[i].name[j] = '\0';
            vfs_table[i].content[0] = '\0';
            
            print_str("Created file: ", 0x0A);
            print_str(name, 0x0F);
            print_char('\n');
            return;
        }
    }
}

void vfs_rm(const char* name) {
    VFile* f = vfs_get(name);
    if (f) {
        f->used = false;
        f->name[0] = '\0';
        f->content[0] = '\0';
        print_str("Removed file.\n", 0x0A);
    } else {
        print_str("File not found.\n", 0x0C);
    }
}

void vfs_append(const char* name, const char* text) {
    VFile* f = vfs_get(name);
    if (!f) {
        print_str("File not found.\n", 0x0C);
        return;
    }
    int len = 0;
    while (f->content[len] != '\0') len++;
    int i = 0;
    while (text[i] != '\0' && (len + i) < FILE_SIZE - 2) {
        f->content[len + i] = text[i];
        i++;
    }
    f->content[len + i] = '\n';
    f->content[len + i + 1] = '\0';
    print_str("Appended line to file.\n", 0x0A);
}

// --- FLUX INTERPRETER STATE & PARSER ---
struct Variable {
    char name[32];
    int value;
};

Variable var_table[MAX_VARS];
int var_count = 0;

int get_var(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) return var_table[i].value;
    }
    return 0;
}

void set_var(const char* name, int val) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_table[i].name, name) == 0) {
            var_table[i].value = val;
            return;
        }
    }
    if (var_count < MAX_VARS) {
        int idx = 0;
        while (name[idx] && idx < 31) { var_table[var_count].name[idx] = name[idx]; idx++; }
        var_table[var_count].name[idx] = '\0';
        var_table[var_count].value = val;
        var_count++;
    }
}

int eval_expr(const char* expr) {
    while (*expr == ' ') expr++;
    char op = 0;
    const char* op_ptr = 0;
    for (int i = 0; expr[i] != '\0'; i++) {
        if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*') {
            op = expr[i];
            op_ptr = &expr[i];
            break;
        }
    }

    if (op_ptr) {
        char left_str[32], right_str[32];
        int idx = 0;
        for (const char* p = expr; p < op_ptr; p++) {
            if (*p != ' ') left_str[idx++] = *p;
        }
        left_str[idx] = '\0';
        
        idx = 0;
        for (const char* p = op_ptr + 1; *p != '\0'; p++) {
            if (*p != ' ') right_str[idx++] = *p;
        }
        right_str[idx] = '\0';

        int left_val = is_digit(left_str[0]) ? atoi(left_str) : get_var(left_str);
        int right_val = is_digit(right_str[0]) ? atoi(right_str) : get_var(right_str);

        if (op == '+') return left_val + right_val;
        if (op == '-') return left_val - right_val;
        if (op == '*') return left_val * right_val;
    }

    if (is_digit(expr[0])) return atoi(expr);
    if (is_alpha(expr[0])) {
        char clean_var[32];
        int idx = 0;
        for (int i = 0; expr[i] != '\0' && expr[i] != ' '; i++) clean_var[idx++] = expr[i];
        clean_var[idx] = '\0';
        return get_var(clean_var);
    }
    return 0;
}

bool in_flux_repl = false;

void eval_flux(const char* code) {
    if (strcmp(code, "exit") == 0) {
        in_flux_repl = false;
        print_str("Exited Flux REPL.\n", 0x0E);
        return;
    }

    if (code[0] == '\0') return;

    // Control Flow: IF <var/val> <op> <var/val> <cmd>
    if (strncmp(code, "if ", 3) == 0) {
        const char* p = code + 3;
        char left[32], right[32], op[3];
        int idx = 0;
        while (*p && *p != ' ' && idx < 31) left[idx++] = *p++; left[idx] = '\0';
        if (*p == ' ') p++;
        
        idx = 0;
        while (*p && *p != ' ' && idx < 2) op[idx++] = *p++; op[idx] = '\0';
        if (*p == ' ') p++;

        idx = 0;
        while (*p && *p != ' ' && idx < 31) right[idx++] = *p++; right[idx] = '\0';
        if (*p == ' ') p++;

        int l_val = is_digit(left[0]) ? atoi(left) : get_var(left);
        int r_val = is_digit(right[0]) ? atoi(right) : get_var(right);

        bool condition = false;
        if (strcmp(op, ">") == 0) condition = l_val > r_val;
        else if (strcmp(op, "<") == 0) condition = l_val < r_val;
        else if (strcmp(op, "==") == 0) condition = l_val == r_val;

        if (condition) {
            eval_flux(p);
        }
        return;
    }

    // Graphics Draw Command: rect x y w h color
    if (strncmp(code, "rect ", 5) == 0) {
        int x = 0, y = 0, w = 0, h = 0, color = 15;
        const char* p = code + 5;
        x = atoi(p); while (*p && *p != ' ') p++; while (*p == ' ') p++;
        y = atoi(p); while (*p && *p != ' ') p++; while (*p == ' ') p++;
        w = atoi(p); while (*p && *p != ' ') p++; while (*p == ' ') p++;
        h = atoi(p); while (*p && *p != ' ') p++; while (*p == ' ') p++;
        color = atoi(p);
        
        draw_rect(x, y, w, h, (uint8_t)color);
        print_str(">> [rect rendered]\n", 0x0A);
        return;
    }

    // Assignment
    const char* eq_ptr = 0;
    for (int i = 0; code[i] != '\0'; i++) {
        if (code[i] == '=' && code[i+1] != '=') { eq_ptr = &code[i]; break; }
    }

    if (eq_ptr) {
        char var_name[32];
        int idx = 0;
        for (const char* p = code; p < eq_ptr; p++) {
            if (*p != ' ') var_name[idx++] = *p;
        }
        var_name[idx] = '\0';

        int val = eval_expr(eq_ptr + 1);
        set_var(var_name, val);

        print_str(">> set ", 0x0A);
        print_str(var_name, 0x0F);
        print_str(" = ", 0x0A);
        char num_buf[16];
        itoa(val, num_buf);
        print_str(num_buf, 0x0F);
        print_char('\n');
        return;
    }

    // Print
    if (strncmp(code, "print ", 6) == 0) {
        int res = eval_expr(code + 6);
        print_str(">> ", 0x0A);
        char num_buf[16];
        itoa(res, num_buf);
        print_str(num_buf, 0x0F);
        print_char('\n');
        return;
    }

    print_str(">> ", 0x0A);
    print_str(code, 0x0F);
    print_char('\n');
}

void exec_vfs_file(const char* filename) {
    VFile* file = vfs_get(filename);
    if (!file) {
        print_str("File not found: ", 0x0C);
        print_str(filename, 0x0C);
        print_char('\n');
        return;
    }

    print_str("--- Executing ", 0x0B);
    print_str(filename, 0x0B);
    print_str(" ---\n", 0x0B);

    char line_buf[256];
    int idx = 0;
    for (int i = 0; file->content[i] != '\0'; i++) {
        char c = file->content[i];
        if (c == '\r') continue;

        if (c == '\n') {
            line_buf[idx] = '\0';
            if (idx > 0) eval_flux(line_buf);
            idx = 0;
        } else {
            if (idx < 255) line_buf[idx++] = c;
        }
    }
    if (idx > 0) {
        line_buf[idx] = '\0';
        eval_flux(line_buf);
    }
    print_str("------------------------------\n", 0x0B);
}

// Interactive Text Editor
bool in_editor = false;
VFile* active_editor_file = 0;

void start_editor(const char* name) {
    VFile* f = vfs_get(name);
    if (!f) {
        vfs_touch(name);
        f = vfs_get(name);
    }
    active_editor_file = f;
    in_editor = true;
    print_str("\n--- EDIT MODE (Type ':w' to save/exit) ---\n", 0x0E);
    print_str(f->content, 0x0F);
}

// --- SHELL DISPATCHER ---
void execute_command(char* cmd) {
    if (in_editor) {
        if (strcmp(cmd, ":w") == 0) {
            in_editor = false;
            active_editor_file = 0;
            print_str("\nFile saved.\n", 0x0A);
            return;
        }
        vfs_append(active_editor_file->name, cmd);
        return;
    }

    if (in_flux_repl) {
        eval_flux(cmd);
        return;
    }

    if (strcmp(cmd, "help") == 0) {
        print_str("Available OS Commands:\n", 0x0B);
        print_str("  flux              - Launch Flux REPL\n", 0x0F);
        print_str("  ls                - List files in RAM VFS\n", 0x0F);
        print_str("  cat <f>           - Display file contents\n", 0x0F);
        print_str("  touch <f>         - Create a new script file\n", 0x0F);
        print_str("  edit <f>          - Interactive multi-line editor\n", 0x0F);
        print_str("  add <f> <line>    - Append line to script\n", 0x0F);
        print_str("  rm <f>            - Delete file\n", 0x0F);
        print_str("  exec <f>          - Execute Flux script\n", 0x0F);
        print_str("  gfx               - VGA graphics demo\n", 0x0F);
        print_str("  clear             - Clear terminal screen\n", 0x0F);
        print_str("  mem               - Check memory status\n", 0x0F);
    } else if (strcmp(cmd, "flux") == 0) {
        in_flux_repl = true;
        print_str("=== Flux OS Bare-Metal REPL ===\n", 0x0B);
        print_str("Type 'exit' to return to OS shell.\n\n", 0x0F);
    } else if (strcmp(cmd, "ls") == 0) {
        vfs_list();
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        VFile* f = vfs_get(cmd + 4);
        if (f) {
            print_str(f->content, 0x0F);
            print_char('\n');
        } else {
            print_str("File not found.\n", 0x0C);
        }
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        vfs_touch(cmd + 6);
    } else if (strncmp(cmd, "edit ", 5) == 0) {
        start_editor(cmd + 5);
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        vfs_rm(cmd + 3);
    } else if (strncmp(cmd, "add ", 4) == 0) {
        const char* p = cmd + 4;
        char fname[32];
        int idx = 0;
        while (*p && *p != ' ' && idx < 31) fname[idx++] = *p++;
        fname[idx] = '\0';
        if (*p == ' ') p++;
        vfs_append(fname, p);
    } else if (strncmp(cmd, "exec ", 5) == 0) {
        exec_vfs_file(cmd + 5);
    } else if (strcmp(cmd, "gfx") == 0) {
        demo_graphics();
    } else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } else if (strcmp(cmd, "mem") == 0) {
        print_str("Heap Base: 0x200000 | Dynamic kmalloc Ready\n", 0x0A);
    } else if (cmd[0] != '\0') {
        print_str("Unknown command: ", 0x0C);
        print_str(cmd, 0x0C);
        print_char('\n');
    }
}

// Keyboard Translator
bool shift_active = false;

char get_char_from_scancode(uint8_t scancode) {
    switch(scancode) {
        case 0x2A: case 0x36: shift_active = true; return 0;
        case 0xAA: case 0xB6: shift_active = false; return 0;

        case 0x1E: return shift_active ? 'A' : 'a';
        case 0x30: return shift_active ? 'B' : 'b';
        case 0x2E: return shift_active ? 'C' : 'c';
        case 0x20: return shift_active ? 'D' : 'd';
        case 0x12: return shift_active ? 'E' : 'e';
        case 0x21: return shift_active ? 'F' : 'f';
        case 0x22: return shift_active ? 'G' : 'g';
        case 0x23: return shift_active ? 'H' : 'h';
        case 0x17: return shift_active ? 'I' : 'i';
        case 0x24: return shift_active ? 'J' : 'j';
        case 0x25: return shift_active ? 'K' : 'k';
        case 0x26: return shift_active ? 'L' : 'l';
        case 0x32: return shift_active ? 'M' : 'm';
        case 0x31: return shift_active ? 'N' : 'n';
        case 0x18: return shift_active ? 'O' : 'o';
        case 0x19: return shift_active ? 'P' : 'p';
        case 0x10: return shift_active ? 'Q' : 'q';
        case 0x13: return shift_active ? 'R' : 'r';
        case 0x1F: return shift_active ? 'S' : 's';
        case 0x14: return shift_active ? 'T' : 't';
        case 0x16: return shift_active ? 'U' : 'u';
        case 0x2F: return shift_active ? 'V' : 'v';
        case 0x11: return shift_active ? 'W' : 'w';
        case 0x2D: return shift_active ? 'X' : 'x';
        case 0x15: return shift_active ? 'Y' : 'y';
        case 0x2C: return shift_active ? 'Z' : 'z';

        case 0x02: return shift_active ? '!' : '1';
        case 0x03: return shift_active ? '@' : '2';
        case 0x04: return shift_active ? '#' : '3';
        case 0x05: return shift_active ? '$' : '4';
        case 0x06: return shift_active ? '%' : '5';
        case 0x07: return shift_active ? '^' : '6';
        case 0x08: return shift_active ? '&' : '7';
        case 0x09: return shift_active ? '*' : '8';
        case 0x0A: return shift_active ? '(' : '9';
        case 0x0B: return shift_active ? ')' : '0';

        case 0x33: return shift_active ? '<' : ',';
        case 0x34: return shift_active ? '>' : '.';
        case 0x35: return shift_active ? '?' : '/';
        case 0x27: return shift_active ? ':' : ';';
        case 0x28: return shift_active ? '"' : '\'';
        case 0x0C: return shift_active ? '_' : '-';
        case 0x0D: return shift_active ? '+' : '=';
        case 0x1A: return shift_active ? '{' : '[';
        case 0x1B: return shift_active ? '}' : ']';

        case 0x39: return ' ';
        case 0x1C: return '\n';
        case 0x0E: return '\b';
        default: return 0;
    }
}

extern "C" void kernel_main() {
    clear_screen();
    vfs_init();
    mouse_init();

    print_str("===================================\n", 0x0B);
    print_str("       FLUX BARE-METAL OS v0.9     \n", 0x0A);
    print_str("===================================\n\n", 0x0B);
    print_str("Type 'help' to view system commands.\n\n", 0x0F);
    print_str("flux_os> ", 0x0E);

    char cmd_buf[MAX_CMD_LEN];
    int buf_idx = 0;
    uint8_t last_scancode = 0;

    while(1) {
        poll_mouse();

        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode != last_scancode) {
                char ch = get_char_from_scancode(scancode);
                if (!(scancode & 0x80)) {
                    if (ch == '\n') {
                        print_char('\n');
                        cmd_buf[buf_idx] = '\0';
                        execute_command(cmd_buf);
                        buf_idx = 0;
                        if (in_editor) {
                            print_str("edit> ", 0x0E);
                        } else if (in_flux_repl) {
                            print_str("flux> ", 0x0B);
                        } else {
                            print_str("flux_os> ", 0x0E);
                        }
                    } else if (ch == '\b') {
                        if (buf_idx > 0) {
                            buf_idx--;
                            print_char('\b');
                        }
                    } else if (ch != 0 && buf_idx < MAX_CMD_LEN - 1) {
                        cmd_buf[buf_idx++] = ch;
                        print_char(ch, 0x0F);
                    }
                }
            }
            last_scancode = scancode;
        }
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define CLEAR "cls"
#else
#define CLEAR "clear"
#endif

#define DATA_FILE "expenses.csv"
#define MAX_CATEGORY 32
#define MAX_NOTE 80
#define MAX_DATE  11   // "YYYY-MM-DD" + '\0'
#define LINE_BUF  512

typedef struct {
    int id;
    char date[MAX_DATE];           // YYYY-MM-DD
    char category[MAX_CATEGORY];
    char note[MAX_NOTE];
    double amount;
} Expense;

/* ---------- Utilities ---------- */

int file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void ensure_csv_exists() {
    if (!file_exists(DATA_FILE)) {
        FILE *f = fopen(DATA_FILE, "w");
        if (!f) {
            printf("Error: cannot create %s\n", DATA_FILE);
            exit(1);
        }
        fprintf(f, "id,date,category,note,amount\n");
        fclose(f);
    }
}

// Simple date format check: "YYYY-MM-DD" with basic range checks
int valid_date(const char *s) {
    if (strlen(s) != 10) return 0;
    if (s[4] != '-' || s[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    int y = atoi(s);
    int m = atoi(s + 5);
    int d = atoi(s + 8);
    if (y < 1900 || y > 3000) return 0;
    if (m < 1 || m > 12) return 0;
    int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
    if (leap) mdays[2] = 29;
    if (d < 1 || d > mdays[m]) return 0;
    return 1;
}

void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[n-1] = '\0';
}

void read_line(char *buf, size_t size) {
    if (fgets(buf, (int)size, stdin) == NULL) {
        // EOF or error, safeguard:
        buf[0] = '\0';
        return;
    }
    trim_newline(buf);
}

int prompt_int(const char *msg) {
    char buf[64];
    printf("%s", msg);
    read_line(buf, sizeof(buf));
    return atoi(buf);
}

double prompt_double(const char *msg) {
    char buf[64];
    printf("%s", msg);
    read_line(buf, sizeof(buf));
    return atof(buf);
}

void prompt_str(const char *msg, char *out, size_t outsz) {
    printf("%s", msg);
    read_line(out, outsz);
}

int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ---------- CSV Load/Save ---------- */

int parse_expense_line(const char *line, Expense *e) {
    // Expect CSV: id,date,category,note,amount
    // Handle simple CSV without embedded commas in fields.
    char tmp[LINE_BUF];
    strncpy(tmp, line, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';

    char *p = tmp;
    char *tok;
    int col = 0;

    while ((tok = strsep(&p, ",")) != NULL) {
        switch (col) {
            case 0: e->id = atoi(tok); break;
            case 1: strncpy(e->date, tok, MAX_DATE); e->date[MAX_DATE-1] = '\0'; break;
            case 2: strncpy(e->category, tok, MAX_CATEGORY); e->category[MAX_CATEGORY-1] = '\0'; break;
            case 3: strncpy(e->note, tok, MAX_NOTE); e->note[MAX_NOTE-1] = '\0'; break;
            case 4: e->amount = atof(tok); break;
            default: break;
        }
        col++;
    }
    return (col >= 5);
}

int load_all(Expense **arr) {
    ensure_csv_exists();
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) {
        printf("Error: cannot open %s\n", DATA_FILE);
        return 0;
    }
    char line[LINE_BUF];
    int count = 0;
    int cap = 64;
    *arr = (Expense*)malloc(sizeof(Expense) * cap);
    if (!*arr) {
        fclose(f);
        printf("Error: out of memory\n");
        return 0;
    }

    // skip header
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (line[0] == '\0') continue;
        if (count >= cap) {
            cap *= 2;
            Expense *tmp = (Expense*)realloc(*arr, sizeof(Expense)*cap);
            if (!tmp) { fclose(f); free(*arr); printf("Error: out of memory\n"); return 0; }
            *arr = tmp;
        }
        if (parse_expense_line(line, &((*arr)[count]))) {
            count++;
        }
    }
    fclose(f);
    return count;
}

int get_next_id() {
    Expense *arr = NULL;
    int n = load_all(&arr);
    int maxid = 0;
    for (int i = 0; i < n; i++) if (arr[i].id > maxid) maxid = arr[i].id;
    free(arr);
    return maxid + 1;
}

int save_all(Expense *arr, int n) {
    FILE *f = fopen(DATA_FILE, "w");
    if (!f) {
        printf("Error: cannot write %s\n", DATA_FILE);
        return 0;
    }
    fprintf(f, "id,date,category,note,amount\n");
    for (int i = 0; i < n; i++) {
        fprintf(f, "%d,%s,%s,%s,%.2f\n",
                arr[i].id, arr[i].date, arr[i].category, arr[i].note, arr[i].amount);
    }
    fclose(f);
    return 1;
}

void append_expense(const Expense *e) {
    ensure_csv_exists();
    FILE *f = fopen(DATA_FILE, "a");
    if (!f) {
        printf("Error: cannot append to %s\n", DATA_FILE);
        return;
    }
    fprintf(f, "%d,%s,%s,%s,%.2f\n", e->id, e->date, e->category, e->note, e->amount);
    fclose(f);
}

/* ---------- Features ---------- */

void add_expense() {
    Expense e;
    e.id = get_next_id();

    do {
        prompt_str("Enter date (YYYY-MM-DD): ", e.date, sizeof(e.date));
        if (!valid_date(e.date)) printf("Invalid date format. Try again.\n");
    } while (!valid_date(e.date));

    prompt_str("Enter category (e.g., Food, Travel, Bills): ", e.category, sizeof(e.category));
    prompt_str("Enter note (short description): ", e.note, sizeof(e.note));
    e.amount = prompt_double("Enter amount: ");

    append_expense(&e);
    printf("\n✅ Added expense with ID %d\n", e.id);
}

void print_table_header() {
    printf("--------------------------------------------------------------------------------\n");
    printf("%-5s  %-10s  %-15s  %-30s  %10s\n", "ID", "Date", "Category", "Note", "Amount");
    printf("--------------------------------------------------------------------------------\n");
}

void list_expenses() {
    Expense *arr = NULL;
    int n = load_all(&arr);
    if (n == 0) { printf("\nNo expenses found.\n"); free(arr); return; }

    print_table_header();
    double total = 0.0;
    for (int i = 0; i < n; i++) {
        printf("%-5d  %-10s  %-15s  %-30s  %10.2f\n",
               arr[i].id, arr[i].date, arr[i].category, arr[i].note, arr[i].amount);
        total += arr[i].amount;
    }
    printf("--------------------------------------------------------------------------------\n");
    printf("%-64s %10.2f\n", "TOTAL", total);
    free(arr);
}

void search_expenses() {
    char q[64];
    prompt_str("Search keyword (in category/note): ", q, sizeof(q));

    Expense *arr = NULL;
    int n = load_all(&arr);
    int found = 0;
    double total = 0.0;

    print_table_header();
    for (int i = 0; i < n; i++) {
        if (strstr(arr[i].category, q) || strstr(arr[i].note, q)) {
            printf("%-5d  %-10s  %-15s  %-30s  %10.2f\n",
                   arr[i].id, arr[i].date, arr[i].category, arr[i].note, arr[i].amount);
            found++;
            total += arr[i].amount;
        }
    }
    if (!found) printf("No matching records.\n");
    else {
        printf("--------------------------------------------------------------------------------\n");
        printf("%-64s %10.2f\n", "SUBTOTAL (matches)", total);
    }
    free(arr);
}

void monthly_summary() {
    char ym[8]; // YYYY-MM
    prompt_str("Enter month (YYYY-MM): ", ym, sizeof(ym));

    if (strlen(ym) != 7 || ym[4] != '-') {
        printf("Invalid month format.\n");
        return;
    }

    Expense *arr = NULL;
    int n = load_all(&arr);
    int found = 0;
    double total = 0.0;

    print_table_header();
    for (int i = 0; i < n; i++) {
        if (starts_with(arr[i].date, ym)) {
            printf("%-5d  %-10s  %-15s  %-30s  %10.2f\n",
                   arr[i].id, arr[i].date, arr[i].category, arr[i].note, arr[i].amount);
            found++;
            total += arr[i].amount;
        }
    }
    if (!found) printf("No expenses for %s.\n", ym);
    else {
        printf("--------------------------------------------------------------------------------\n");
        printf("%-64s %10.2f\n", "MONTH TOTAL", total);
    }
    free(arr);
}

void category_totals() {
    Expense *arr = NULL;
    int n = load_all(&arr);
    if (n == 0) { printf("\nNo expenses found.\n"); free(arr); return; }

    // Simple aggregation in memory (case-sensitive categories)
    typedef struct { char cat[MAX_CATEGORY]; double sum; } Row;
    Row *rows = NULL; int rc = 0, cap = 16;
    rows = (Row*)malloc(sizeof(Row)*cap);

    for (int i = 0; i < n; i++) {
        int idx = -1;
        for (int j = 0; j < rc; j++) {
            if (strcmp(rows[j].cat, arr[i].category) == 0) { idx = j; break; }
        }
        if (idx == -1) {
            if (rc >= cap) {
                cap *= 2;
                Row *tmp = (Row*)realloc(rows, sizeof(Row)*cap);
                if (!tmp) { printf("OOM\n"); free(rows); free(arr); return; }
                rows = tmp;
            }
            strncpy(rows[rc].cat, arr[i].category, MAX_CATEGORY);
            rows[rc].cat[MAX_CATEGORY-1] = '\0';
            rows[rc].sum = arr[i].amount;
            rc++;
        } else {
            rows[idx].sum += arr[i].amount;
        }
    }

    printf("\n----------------------------\n");
    printf("%-20s %12s\n", "Category", "Total");
    printf("----------------------------\n");
    double grand = 0.0;
    for (int i = 0; i < rc; i++) {
        printf("%-20s %12.2f\n", rows[i].cat, rows[i].sum);
        grand += rows[i].sum;
    }
    printf("----------------------------\n");
    printf("%-20s %12.2f\n", "GRAND TOTAL", grand);

    free(rows);
    free(arr);
}

void delete_expense() {
    int target = prompt_int("Enter ID to delete: ");
    Expense *arr = NULL;
    int n = load_all(&arr);
    if (n == 0) { printf("No data.\n"); free(arr); return; }

    int kept = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i].id != target) arr[kept++] = arr[i];
    }
    if (kept == n) {
        printf("ID %d not found.\n", target);
        free(arr);
        return;
    }
    if (save_all(arr, kept)) printf("✅ Deleted ID %d\n", target);
    free(arr);
}

void edit_expense() {
    int target = prompt_int("Enter ID to edit: ");
    Expense *arr = NULL;
    int n = load_all(&arr);
    if (n == 0) { printf("No data.\n"); free(arr); return; }

    int idx = -1;
    for (int i = 0; i < n; i++) if (arr[i].id == target) { idx = i; break; }
    if (idx == -1) { printf("ID %d not found.\n", target); free(arr); return; }

    printf("Leave a field empty to keep current value.\n");

    char buf[128];

    // date
    printf("Date [%s] (YYYY-MM-DD): ", arr[idx].date);
    read_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        if (!valid_date(buf)) { printf("Invalid date. Edit cancelled.\n"); free(arr); return; }
        strncpy(arr[idx].date, buf, sizeof(arr[idx].date));
        arr[idx].date[sizeof(arr[idx].date)-1] = '\0';
    }

    // category
    printf("Category [%s]: ", arr[idx].category);
    read_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        strncpy(arr[idx].category, buf, sizeof(arr[idx].category));
        arr[idx].category[sizeof(arr[idx].category)-1] = '\0';
    }

    // note
    printf("Note [%s]: ", arr[idx].note);
    read_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        strncpy(arr[idx].note, buf, sizeof(arr[idx].note));
        arr[idx].note[sizeof(arr[idx].note)-1] = '\0';
    }

    // amount
    printf("Amount [%.2f]: ", arr[idx].amount);
    read_line(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        arr[idx].amount = atof(buf);
    }

    if (save_all(arr, n)) printf("✅ Updated ID %d\n", target);
    free(arr);
}

/* ---------- Menu ---------- */

void pause_enter() {
    printf("\nPress ENTER to continue...");
    char cbuf[8];
    fgets(cbuf, sizeof(cbuf), stdin);
}

void menu() {
    while (1) {
        printf("\n=========== Expense Tracker ===========\n");
        printf("1) Add Expense\n");
        printf("2) List All Expenses\n");
        printf("3) Search (category/note)\n");
        printf("4) Monthly Summary (YYYY-MM)\n");
        printf("5) Category Totals\n");
        printf("6) Edit Expense by ID\n");
        printf("7) Delete Expense by ID\n");
        printf("0) Exit\n");
        printf("======================================\n");

        int ch = prompt_int("Choose an option: ");
        printf("\n");

        switch (ch) {
            case 1: add_expense(); break;
            case 2: list_expenses(); break;
            case 3: search_expenses(); break;
            case 4: monthly_summary(); break;
            case 5: category_totals(); break;
            case 6: edit_expense(); break;
            case 7: delete_expense(); break;
            case 0: printf("Goodbye!\n"); return;
            default: printf("Invalid choice.\n");
        }
        pause_enter();
        system(CLEAR);
    }
}

int main(void) {
    ensure_csv_exists();
    menu();
    return 0;
}

/* expense_tracker.c
 * A simple CLI Expense Tracker in C with CSV persistence.
 * Features: Add, List, Search, Edit, Delete, Monthly Summary, Category Totals.
 *
 * Storage file: expenses.csv (created automatically with a header).
 *
 * Compile: gcc expense_tracker.c -o expense_tracker
 */

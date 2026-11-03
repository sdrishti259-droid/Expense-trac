/*
 * ============================================================
 *  EXPENSE TRACKER  -  a menu-driven project in C
 * ============================================================
 *  Features:
 *    1. Add expense (ID is generated automatically)
 *    2. View all expenses + total
 *    3. Search (by ID, title keyword, or category)
 *    4. Update an expense
 *    5. Delete an expense
 *    6. Category-wise summary
 *    7. Set monthly budget (warns you when you cross it)
 *    8. Export report to report.txt
 *    9. Exit
 *   10. Load sample data (20 demo expenses)
 *
 *  Concepts used: struct, functions, file handling (binary),
 *                 arrays, loops, switch-case, input validation
 *
 *  Compile:  gcc expense_tracker.c -o tracker
 *  Run:      ./tracker        (Windows: tracker.exe)
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- Constants ---------- */
#define FILE_NAME    "expenses.dat"   /* stores all expenses            */
#define TEMP_FILE    "temp.dat"       /* used while deleting records    */
#define BUDGET_FILE  "budget.dat"     /* stores the monthly budget      */
#define REPORT_FILE  "report.txt"     /* the exported report            */

#define NUM_CATEGORIES 5
const char *CATEGORIES[NUM_CATEGORIES] = {
    "Food", "Travel", "Study", "Shopping", "Other"
};

/* ---------- The struct: one expense = one "form" ---------- */
struct Expense {
    int   id;
    char  title[50];
    char  category[30];
    float amount;
    char  date[11];      /* DD-MM-YYYY plus the '\0' at the end */
};

/* ============================================================
 *  PART 1: SAFE INPUT HELPERS
 *  (scanf can break easily, so we read whole lines with fgets)
 * ============================================================ */

/* Removes spaces/newlines from the start and end of a string */
void trim(char *s) {
    int start = 0, end = (int)strlen(s) - 1;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    while (end >= start && isspace((unsigned char)s[end])) end--;
    int len = end - start + 1;
    if (len < 0) len = 0;
    memmove(s, s + start, len);
    s[len] = '\0';
}

/* Prints a prompt and reads one full line into buf (max size-1 chars) */
void readLine(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (fgets(buf, size, stdin) == NULL) {   /* input closed (Ctrl+D / Ctrl+Z) */
        printf("\nInput ended. Exiting.\n");
        exit(0);
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        int c;                                /* line was too long: throw away the rest */
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    trim(buf);
}

/* Keeps asking until the user types a valid whole number */
int readInt(const char *prompt) {
    char buf[64], *end;
    while (1) {
        readLine(prompt, buf, sizeof(buf));
        long n = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0') return (int)n;
        printf("  Please enter a valid number.\n");
    }
}

/* Converts text to a positive amount. Returns 1 if valid, else 0 */
int parseAmount(const char *s, float *out) {
    char *end;
    double v = strtod(s, &end);
    if (s[0] == '\0' || *end != '\0') return 0;
    if (v <= 0 || v > 10000000) return 0;
    *out = (float)v;
    return 1;
}

/* Keeps asking until the user types a valid positive amount */
float readAmount(const char *prompt) {
    char buf[64];
    float v;
    while (1) {
        readLine(prompt, buf, sizeof(buf));
        if (parseAmount(buf, &v)) return v;
        printf("  Amount must be a number greater than 0.\n");
    }
}

/* Reads a non-empty piece of text (like the title) */
void readNonEmpty(const char *prompt, char *buf, int size) {
    while (1) {
        readLine(prompt, buf, size);
        if (buf[0] != '\0') return;
        printf("  This field cannot be empty.\n");
    }
}

/* Lets the user pick a category from a list.
 * allowKeep = 1 -> pressing Enter keeps the old value (used in update).
 * Returns 1 if a category was chosen, 0 if the user chose to keep. */
int chooseCategory(char *out, int allowKeep) {
    char buf[64], *end;
    int i;

    printf("Categories:\n");
    for (i = 0; i < NUM_CATEGORIES; i++)
        printf("  %d. %s\n", i + 1, CATEGORIES[i]);

    while (1) {
        readLine(allowKeep ? "Choose category (Enter = keep current): "
                           : "Choose category (1-5): ",
                 buf, sizeof(buf));
        if (allowKeep && buf[0] == '\0') return 0;

        long n = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && n >= 1 && n <= NUM_CATEGORIES) {
            strcpy(out, CATEGORIES[n - 1]);
            return 1;
        }
        printf("  Please enter a number from 1 to %d.\n", NUM_CATEGORIES);
    }
}

/* ============================================================
 *  PART 2: DATE CHECKING
 * ============================================================ */

int isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int daysInMonth(int m, int y) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeapYear(y)) return 29;
    return days[m - 1];
}

/* Checks the format DD-MM-YYYY and that the date really exists */
int isValidDate(const char *s) {
    int i;
    if (strlen(s) != 10) return 0;
    for (i = 0; i < 10; i++) {
        if (i == 2 || i == 5) { if (s[i] != '-') return 0; }
        else if (!isdigit((unsigned char)s[i])) return 0;
    }
    int d = (s[0] - '0') * 10 + (s[1] - '0');
    int m = (s[3] - '0') * 10 + (s[4] - '0');
    int y = (s[6] - '0') * 1000 + (s[7] - '0') * 100 +
            (s[8] - '0') * 10 + (s[9] - '0');
    if (y < 2000 || y > 2100) return 0;
    if (m < 1 || m > 12) return 0;
    if (d < 1 || d > daysInMonth(m, y)) return 0;
    return 1;
}

/* allowKeep = 1 -> pressing Enter keeps the old date (used in update).
 * Returns 1 if a new date was typed, 0 if kept. */
int readDate(const char *prompt, char *out, int allowKeep) {
    char buf[64];
    while (1) {
        readLine(prompt, buf, sizeof(buf));
        if (allowKeep && buf[0] == '\0') return 0;
        if (isValidDate(buf)) { strcpy(out, buf); return 1; }
        printf("  Invalid date. Use DD-MM-YYYY (example: 25-10-2026).\n");
    }
}

/* ============================================================
 *  PART 3: SMALL UTILITY FUNCTIONS
 * ============================================================ */

/* Case-insensitive "does text contain word?" (Pizza matches "piz") */
int containsIgnoreCase(const char *text, const char *word) {
    char t[100], w[100];
    int i;
    strncpy(t, text, sizeof(t) - 1); t[sizeof(t) - 1] = '\0';
    strncpy(w, word, sizeof(w) - 1); w[sizeof(w) - 1] = '\0';
    for (i = 0; t[i]; i++) t[i] = (char)tolower((unsigned char)t[i]);
    for (i = 0; w[i]; i++) w[i] = (char)tolower((unsigned char)w[i]);
    return strstr(t, w) != NULL;
}

/* Finds the biggest ID in the file and returns +1 (auto-ID) */
int getNextId(void) {
    struct Expense e;
    int maxId = 0;
    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) return 1;                 /* no file yet -> first ID is 1 */
    while (fread(&e, sizeof(e), 1, fp) == 1)
        if (e.id > maxId) maxId = e.id;
    fclose(fp);
    return maxId + 1;
}

/* Prints the column titles of the table (to the screen or a file) */
void printHeader(FILE *out) {
    fprintf(out, "\n%-5s %-22s %-12s %12s  %-12s\n",
            "ID", "Title", "Category", "Amount", "Date");
    fprintf(out, "-----------------------------------------------------------------\n");
}

/* Prints one expense as one table row */
void printRow(FILE *out, const struct Expense *e) {
    fprintf(out, "%-5d %-22s %-12s %12.2f  %-12s\n",
            e->id, e->title, e->category, e->amount, e->date);
}

/* ============================================================
 *  PART 4: BUDGET
 * ============================================================ */

float loadBudget(void) {
    float b = 0;
    FILE *fp = fopen(BUDGET_FILE, "rb");
    if (fp != NULL) {
        if (fread(&b, sizeof(b), 1, fp) != 1) b = 0;
        fclose(fp);
    }
    return b;
}

void saveBudget(float b) {
    FILE *fp = fopen(BUDGET_FILE, "wb");
    if (fp == NULL) { printf("Could not save budget!\n"); return; }
    fwrite(&b, sizeof(b), 1, fp);
    fclose(fp);
}

/* Total spent in one month. monthYear looks like "10-2026" */
float monthTotal(const char *monthYear) {
    struct Expense e;
    float total = 0;
    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) return 0;
    while (fread(&e, sizeof(e), 1, fp) == 1)
        if (strncmp(e.date + 3, monthYear, 7) == 0)   /* e.date+3 skips "DD-" */
            total += e.amount;
    fclose(fp);
    return total;
}

void setBudget(void) {
    float current = loadBudget();
    if (current > 0) printf("Current monthly budget: %.2f\n", current);
    else             printf("No budget set yet.\n");
    float b = readAmount("Enter new monthly budget: ");
    saveBudget(b);
    printf("Budget saved: %.2f per month.\n", b);
}

/* ============================================================
 *  PART 5: THE MAIN FEATURES (CRUD)
 * ============================================================ */

/* ---- CREATE ---- */
void addExpense(void) {
    struct Expense e;
    memset(&e, 0, sizeof(e));                 /* start with a clean, empty form */

    e.id = getNextId();
    printf("\n--- Add New Expense (ID will be %d) ---\n", e.id);

    readNonEmpty("Title (what did you spend on?): ", e.title, sizeof(e.title));
    chooseCategory(e.category, 0);
    e.amount = readAmount("Amount: ");
    readDate("Date (DD-MM-YYYY): ", e.date, 0);

    FILE *fp = fopen(FILE_NAME, "ab");        /* "ab" = add to the end */
    if (fp == NULL) {
        printf("Error: could not open the data file!\n");
        return;
    }
    fwrite(&e, sizeof(e), 1, fp);
    fclose(fp);
    printf("Expense saved successfully!\n");

    /* Budget check for the month of this expense */
    float budget = loadBudget();
    if (budget > 0) {
        float spent = monthTotal(e.date + 3);
        if (spent > budget)
            printf("WARNING: You crossed your budget for %s! Spent %.2f of %.2f.\n",
                   e.date + 3, spent, budget);
        else if (spent >= 0.8f * budget)
            printf("Heads up: you have used %.0f%% of your budget for %s.\n",
                   spent / budget * 100, e.date + 3);
    }
}

/* ---- READ (all) ---- */
/* Prints the whole table to 'out'. Returns how many rows were printed. */
int printAllExpenses(FILE *out) {
    struct Expense e;
    float total = 0;
    int count = 0;

    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) return 0;

    while (fread(&e, sizeof(e), 1, fp) == 1) {
        if (count == 0) printHeader(out);     /* print header only once */
        printRow(out, &e);
        total += e.amount;
        count++;
    }
    fclose(fp);

    if (count > 0) {
        fprintf(out, "-----------------------------------------------------------------\n");
        fprintf(out, "%-5s %-22s %-12s %12.2f\n", "", "TOTAL SPENT", "", total);
        fprintf(out, "Number of expenses: %d\n", count);
    }
    return count;
}

void viewExpenses(void) {
    printf("\n--- All Expenses ---");
    if (printAllExpenses(stdout) == 0)
        printf("\nNo expenses found yet. Add one first!\n");
}

/* ---- READ (search) ---- */
void searchExpense(void) {
    struct Expense e;
    char word[64];
    int found = 0, id;

    printf("\n--- Search ---\n");
    printf("1. By ID\n2. By title keyword\n3. By category\n");
    int choice = readInt("Choose: ");

    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) { printf("No expenses found yet.\n"); return; }

    if (choice == 1) {
        id = readInt("Enter ID: ");
        while (fread(&e, sizeof(e), 1, fp) == 1) {
            if (e.id == id) {
                printHeader(stdout); printRow(stdout, &e);
                found = 1; break;
            }
        }
    } else if (choice == 2) {
        readNonEmpty("Enter keyword: ", word, sizeof(word));
        while (fread(&e, sizeof(e), 1, fp) == 1) {
            if (containsIgnoreCase(e.title, word)) {
                if (!found) printHeader(stdout);
                printRow(stdout, &e);
                found++;
            }
        }
    } else if (choice == 3) {
        char cat[30];
        chooseCategory(cat, 0);
        while (fread(&e, sizeof(e), 1, fp) == 1) {
            if (strcmp(e.category, cat) == 0) {
                if (!found) printHeader(stdout);
                printRow(stdout, &e);
                found++;
            }
        }
    } else {
        printf("Invalid choice!\n");
        fclose(fp);
        return;
    }
    fclose(fp);

    if (!found) printf("No matching expense found.\n");
}

/* ---- UPDATE ---- */
void updateExpense(void) {
    struct Expense e;
    int id, found = 0;

    printf("\n--- Update Expense ---\n");
    id = readInt("Enter the ID to update: ");

    FILE *fp = fopen(FILE_NAME, "r+b");       /* "r+b" = read AND write */
    if (fp == NULL) { printf("No expenses found yet.\n"); return; }

    while (fread(&e, sizeof(e), 1, fp) == 1) {
        if (e.id == id) { found = 1; break; }
    }

    if (!found) {
        printf("No expense with ID %d.\n", id);
        fclose(fp);
        return;
    }

    printf("Current record:");
    printHeader(stdout); printRow(stdout, &e);
    printf("(Press Enter on any field to keep the old value)\n");

    char buf[64];
    float newAmount;

    readLine("New title: ", buf, sizeof(e.title));
    if (buf[0] != '\0') strcpy(e.title, buf);

    chooseCategory(e.category, 1);

    while (1) {
        readLine("New amount: ", buf, sizeof(buf));
        if (buf[0] == '\0') break;                        /* keep old */
        if (parseAmount(buf, &newAmount)) { e.amount = newAmount; break; }
        printf("  Amount must be a number greater than 0.\n");
    }

    readDate("New date (DD-MM-YYYY): ", e.date, 1);

    /* Move back by one record, then overwrite it with the new data */
    fseek(fp, -(long)sizeof(e), SEEK_CUR);
    fwrite(&e, sizeof(e), 1, fp);
    fclose(fp);
    printf("Expense updated successfully!\n");
}

/* ---- DELETE ---- */
void deleteExpense(void) {
    struct Expense e;
    int id, found = 0;
    char ans[16];

    printf("\n--- Delete Expense ---\n");
    id = readInt("Enter the ID to delete: ");

    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) { printf("No expenses found yet.\n"); return; }

    /* First, find it and ask for confirmation */
    while (fread(&e, sizeof(e), 1, fp) == 1) {
        if (e.id == id) { found = 1; break; }
    }
    if (!found) {
        printf("No expense with ID %d.\n", id);
        fclose(fp);
        return;
    }

    printHeader(stdout); printRow(stdout, &e);
    readLine("Are you sure you want to delete this? (y/n): ", ans, sizeof(ans));
    if (tolower((unsigned char)ans[0]) != 'y') {
        printf("Delete cancelled.\n");
        fclose(fp);
        return;
    }

    /* Copy every record EXCEPT the one being deleted into a temp file,
     * then replace the old file with the temp file. */
    rewind(fp);
    FILE *temp = fopen(TEMP_FILE, "wb");
    if (temp == NULL) {
        printf("Error: could not create temp file!\n");
        fclose(fp);
        return;
    }
    while (fread(&e, sizeof(e), 1, fp) == 1) {
        if (e.id != id) fwrite(&e, sizeof(e), 1, temp);
    }
    fclose(fp);
    fclose(temp);

    remove(FILE_NAME);
    rename(TEMP_FILE, FILE_NAME);
    printf("Expense deleted successfully!\n");
}

/* ============================================================
 *  PART 6: REPORTS
 * ============================================================ */

/* Prints how much was spent in each category (to screen or file) */
void printCategorySummary(FILE *out) {
    struct Expense e;
    float sums[NUM_CATEGORIES] = {0};
    float total = 0;
    int i;

    FILE *fp = fopen(FILE_NAME, "rb");
    if (fp == NULL) { fprintf(out, "\nNo expenses found yet.\n"); return; }

    while (fread(&e, sizeof(e), 1, fp) == 1) {
        int idx = NUM_CATEGORIES - 1;                    /* default: "Other" */
        for (i = 0; i < NUM_CATEGORIES; i++)
            if (strcmp(e.category, CATEGORIES[i]) == 0) { idx = i; break; }
        sums[idx] += e.amount;
        total += e.amount;
    }
    fclose(fp);

    fprintf(out, "\n%-12s %12s %10s\n", "Category", "Spent", "Share");
    fprintf(out, "-------------------------------------\n");
    for (i = 0; i < NUM_CATEGORIES; i++) {
        float pct = (total > 0) ? sums[i] / total * 100 : 0;
        fprintf(out, "%-12s %12.2f %9.1f%%\n", CATEGORIES[i], sums[i], pct);
    }
    fprintf(out, "-------------------------------------\n");
    fprintf(out, "%-12s %12.2f\n", "TOTAL", total);
}

void categorySummary(void) {
    printf("\n--- Spending by Category ---");
    printCategorySummary(stdout);
}

void exportReport(void) {
    FILE *out = fopen(REPORT_FILE, "w");
    if (out == NULL) { printf("Could not create %s!\n", REPORT_FILE); return; }

    fprintf(out, "=========== EXPENSE REPORT ===========\n");
    if (printAllExpenses(out) == 0) fprintf(out, "\nNo expenses recorded.\n");
    fprintf(out, "\n--- Spending by Category ---");
    printCategorySummary(out);

    float budget = loadBudget();
    if (budget > 0) fprintf(out, "\nMonthly budget: %.2f\n", budget);

    fclose(out);
    printf("Report saved to %s\n", REPORT_FILE);
}

/* ============================================================
 *  PART 7: SAMPLE DATA (so your demo is never empty)
 * ============================================================ */

/* Adds 20 ready-made expenses to the file, with auto-generated IDs */
void loadSampleData(void) {
    struct Sample {
        const char *title;
        int         cat;      /* 1-5, same numbers as the category menu */
        float       amount;
        const char *date;
    };
    struct Sample samples[] = {
        {"Pizza",               1,  250.00f, "05-10-2026"},
        {"Bus pass",            2,  300.00f, "06-10-2026"},
        {"Python book",         3,  499.00f, "08-10-2026"},
        {"Notebook and pens",   3,   85.00f, "09-10-2026"},
        {"Movie ticket",        5,  220.00f, "11-10-2026"},
        {"Lunch at canteen",    1,   90.00f, "12-10-2026"},
        {"Auto rickshaw",       2,   60.00f, "14-10-2026"},
        {"Headphones",          4,  899.00f, "15-10-2026"},
        {"Online course",       3,  649.00f, "17-10-2026"},
        {"Birthday gift",       5,  350.00f, "19-10-2026"},
        {"Chai and snacks",     1,   60.00f, "21-10-2026"},
        {"Train ticket",        2,  420.00f, "23-10-2026"},
        {"T-shirt",             4,  799.00f, "25-10-2026"},
        {"Dinner with friends", 1,  540.00f, "27-10-2026"},
        {"Phone recharge",      5,  299.00f, "28-10-2026"},
        {"Printout and xerox",  3,   45.00f, "30-10-2026"},
        {"Coffee",              1,  120.00f, "01-11-2026"},
        {"Metro card",          2,  200.00f, "01-11-2026"},
        {"Backpack",            4, 1100.00f, "02-11-2026"},
        {"Stationery",          3,  150.00f, "02-11-2026"}
    };
    int n = (int)(sizeof(samples) / sizeof(samples[0]));
    int i;
    char ans[16];

    if (getNextId() > 1) {
        printf("You already have expenses saved.\n");
        readLine("Add the sample data on top of them? (y/n): ", ans, sizeof(ans));
        if (tolower((unsigned char)ans[0]) != 'y') {
            printf("Cancelled.\n");
            return;
        }
    }

    FILE *fp = fopen(FILE_NAME, "ab");
    if (fp == NULL) { printf("Error: could not open the data file!\n"); return; }

    int nextId = getNextId();
    for (i = 0; i < n; i++) {
        struct Expense e;
        memset(&e, 0, sizeof(e));
        e.id = nextId + i;
        strncpy(e.title, samples[i].title, sizeof(e.title) - 1);
        strcpy(e.category, CATEGORIES[samples[i].cat - 1]);
        e.amount = samples[i].amount;
        strcpy(e.date, samples[i].date);
        fwrite(&e, sizeof(e), 1, fp);
    }
    fclose(fp);
    printf("%d sample expenses added! Choose option 2 to view them.\n", n);
}

/* ============================================================
 *  PART 8: MAIN - the menu that ties everything together
 * ============================================================ */
int main(void) {
    int choice;

    do {
        printf("\n=========== EXPENSE TRACKER ===========\n");
        printf(" 1. Add Expense\n");
        printf(" 2. View All Expenses\n");
        printf(" 3. Search Expense\n");
        printf(" 4. Update Expense\n");
        printf(" 5. Delete Expense\n");
        printf(" 6. Category Summary\n");
        printf(" 7. Set Monthly Budget\n");
        printf(" 8. Export Report to File\n");
        printf(" 9. Exit\n");
        printf("10. Load Sample Data (for demo)\n");
        printf("=======================================\n");
        choice = readInt("Enter your choice (1-10): ");

        switch (choice) {
            case 1: addExpense();      break;
            case 2: viewExpenses();    break;
            case 3: searchExpense();   break;
            case 4: updateExpense();   break;
            case 5: deleteExpense();   break;
            case 6: categorySummary(); break;
            case 7: setBudget();       break;
            case 8: exportReport();    break;
            case 9: printf("Goodbye! Keep tracking your spending.\n"); break;
            case 10: loadSampleData(); break;
            default: printf("Invalid choice! Please enter 1-10.\n");
        }
    } while (choice != 9);

    return 0;
}

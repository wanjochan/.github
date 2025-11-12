/*
 * data_pipeline.c - Data Processing Pipeline Example
 *
 * Demonstrates mod_ds for ETL (Extract-Transform-Load) workflows:
 * - Data loading from structured records
 * - Transformation (filtering, mapping)
 * - Aggregation operations
 * - Multiple output formats (CSV, summary)
 *
 * Build and run:
 *   ./cosmorun.exe c_modules/examples/data_pipeline.c
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_ds.c"

/* ==================== Data Structures ==================== */

// Person record structure
typedef struct {
    char name[64];
    int age;
    char city[64];
    double salary;
} person_t;

// Create a person record
person_t* person_new(const char* name, int age, const char* city, double salary) {
    person_t* p = malloc(sizeof(person_t));
    if (!p) return NULL;

    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->age = age;
    strncpy(p->city, city, sizeof(p->city) - 1);
    p->city[sizeof(p->city) - 1] = '\0';
    p->salary = salary;

    return p;
}

/* ==================== Data Loading ==================== */

/**
 * Load sample data into a list
 * In a real application, this would read from CSV, JSON, or database
 */
ds_list_t* load_sample_data(void) {
    ds_list_t* persons = ds_list_new();

    // Simulate loading data from a data source
    ds_list_push(persons, person_new("Alice", 25, "NYC", 75000.0));
    ds_list_push(persons, person_new("Bob", 17, "LA", 0.0));
    ds_list_push(persons, person_new("Carol", 30, "NYC", 95000.0));
    ds_list_push(persons, person_new("David", 22, "LA", 65000.0));
    ds_list_push(persons, person_new("Eve", 16, "SF", 0.0));
    ds_list_push(persons, person_new("Frank", 35, "NYC", 105000.0));
    ds_list_push(persons, person_new("Grace", 28, "SF", 85000.0));
    ds_list_push(persons, person_new("Henry", 19, "LA", 45000.0));

    printf("[Extract] Loaded %zu records from data source\n", ds_list_size(persons));
    return persons;
}

/* ==================== Data Transformation ==================== */

/**
 * Filter: Keep only records where age >= min_age
 */
ds_list_t* filter_by_age(ds_list_t* persons, int min_age) {
    ds_list_t* filtered = ds_list_new();
    size_t count = ds_list_size(persons);

    for (size_t i = 0; i < count; i++) {
        person_t* p = (person_t*)ds_list_get(persons, i);
        if (p && p->age >= min_age) {
            // Create a copy for filtered list
            person_t* copy = malloc(sizeof(person_t));
            *copy = *p;
            ds_list_push(filtered, copy);
        }
    }

    printf("[Transform] Filtered %zu/%zu records (age >= %d)\n",
           ds_list_size(filtered), count, min_age);
    return filtered;
}

/**
 * Transform: Calculate tax bracket based on salary
 */
const char* get_tax_bracket(double salary) {
    if (salary == 0) return "No income";
    if (salary < 50000) return "Low";
    if (salary < 80000) return "Medium";
    if (salary < 100000) return "High";
    return "Very High";
}

/* ==================== Data Aggregation ==================== */

/**
 * Group persons by city using ds_map
 */
ds_map_t* group_by_city(ds_list_t* persons) {
    ds_map_t* groups = ds_map_new();
    size_t count = ds_list_size(persons);

    for (size_t i = 0; i < count; i++) {
        person_t* p = (person_t*)ds_list_get(persons, i);
        if (!p) continue;

        // Get or create city group
        ds_list_t* city_group = (ds_list_t*)ds_map_get(groups, p->city);
        if (!city_group) {
            city_group = ds_list_new();
            ds_map_put(groups, p->city, city_group);
        }

        // Add person to city group
        person_t* copy = malloc(sizeof(person_t));
        *copy = *p;
        ds_list_push(city_group, copy);
    }

    printf("[Transform] Grouped into %zu cities\n", ds_map_size(groups));
    return groups;
}

/**
 * Calculate and display aggregation statistics per city
 */
void print_aggregation(ds_map_t* groups) {
    printf("\n=== City Aggregation Statistics ===\n");
    printf("%-15s | %8s | %12s | %12s\n", "City", "Count", "Avg Age", "Avg Salary");
    printf("----------------+----------+--------------+--------------\n");

    ds_list_t* cities = ds_map_keys(groups);
    size_t city_count = ds_list_size(cities);

    for (size_t i = 0; i < city_count; i++) {
        const char* city = (const char*)ds_list_get(cities, i);
        ds_list_t* group = (ds_list_t*)ds_map_get(groups, city);

        if (!group) continue;

        int total_age = 0;
        double total_salary = 0.0;
        size_t count = ds_list_size(group);

        for (size_t j = 0; j < count; j++) {
            person_t* p = (person_t*)ds_list_get(group, j);
            if (p) {
                total_age += p->age;
                total_salary += p->salary;
            }
        }

        double avg_age = count > 0 ? (double)total_age / (int)count : 0.0;
        double avg_salary = count > 0 ? total_salary / (int)count : 0.0;

        printf("%-15s | %8zu | %12.1f | $%11.0f\n",
               city, count, avg_age, avg_salary);
    }

    ds_list_free(cities);
}

/* ==================== Output Formats ==================== */

/**
 * Export to CSV format
 */
void export_to_csv(ds_list_t* persons, const char* filename) {
    printf("\n=== CSV Export ===\n");
    printf("name,age,city,salary,tax_bracket\n");

    size_t count = ds_list_size(persons);
    for (size_t i = 0; i < count; i++) {
        person_t* p = (person_t*)ds_list_get(persons, i);
        if (p) {
            printf("%s,%d,%s,%.0f,%s\n",
                   p->name, p->age, p->city, p->salary,
                   get_tax_bracket(p->salary));
        }
    }

    printf("[Load] Exported %zu records to CSV\n", count);
}

/**
 * Generate summary report
 */
void print_summary(ds_list_t* persons, ds_map_t* groups) {
    printf("\n=== Summary Report ===\n");

    // Overall statistics
    size_t total = ds_list_size(persons);
    int total_age = 0;
    double total_salary = 0.0;
    int min_age = 999, max_age = 0;
    double min_salary = 1e9, max_salary = 0.0;

    for (size_t i = 0; i < total; i++) {
        person_t* p = (person_t*)ds_list_get(persons, i);
        if (p) {
            total_age += p->age;
            total_salary += p->salary;

            if (p->age < min_age) min_age = p->age;
            if (p->age > max_age) max_age = p->age;
            if (p->salary > 0 && p->salary < min_salary) min_salary = p->salary;
            if (p->salary > max_salary) max_salary = p->salary;
        }
    }

    double avg_age = total > 0 ? (double)total_age / (int)total : 0.0;
    double avg_salary = total > 0 ? total_salary / (int)total : 0.0;

    printf("  Total Records:     %zu\n", total);
    printf("  Unique Cities:     %zu\n", ds_map_size(groups));
    printf("  Age Range:         %d - %d (avg: %.1f)\n", min_age, max_age, avg_age);
    printf("  Salary Range:      $%.0f - $%.0f (avg: $%.0f)\n",
           min_salary, max_salary, avg_salary);
}

/**
 * Display tax bracket distribution
 */
void print_tax_distribution(ds_list_t* persons) {
    printf("\n=== Tax Bracket Distribution ===\n");

    // Count by bracket using a map
    ds_map_t* brackets = ds_map_new();

    size_t count = ds_list_size(persons);
    for (size_t i = 0; i < count; i++) {
        person_t* p = (person_t*)ds_list_get(persons, i);
        if (!p) continue;

        const char* bracket = get_tax_bracket(p->salary);
        int* cnt = (int*)ds_map_get(brackets, (void*)bracket);
        if (!cnt) {
            cnt = malloc(sizeof(int));
            *cnt = 0;
            ds_map_put(brackets, (void*)bracket, cnt);
        }
        (*cnt)++;
    }

    // Display distribution
    const char* bracket_names[] = {
        "No income", "Low", "Medium", "High", "Very High"
    };

    for (int i = 0; i < 5; i++) {
        int* cnt = (int*)ds_map_get(brackets, bracket_names[i]);
        int c = cnt ? *cnt : 0;
        int pct = count > 0 ? (100 * c) / (int)count : 0;
        printf("  %-12s: %2d (%d%%)\n",
               bracket_names[i], c, pct);
    }

    // Cleanup
    ds_list_t* keys = ds_map_keys(brackets);
    for (size_t i = 0; i < ds_list_size(keys); i++) {
        const char* key = ds_list_get(keys, i);
        free(ds_map_get(brackets, key));
    }
    ds_list_free(keys);
    ds_map_free(brackets);
}

/* ==================== Cleanup ==================== */

void cleanup_persons_list(ds_list_t* list) {
    if (!list) return;

    size_t count = ds_list_size(list);
    for (size_t i = 0; i < count; i++) {
        free(ds_list_get(list, i));
    }
    ds_list_free(list);
}

void cleanup_groups_map(ds_map_t* groups) {
    if (!groups) return;

    ds_list_t* cities = ds_map_keys(groups);
    size_t count = ds_list_size(cities);

    for (size_t i = 0; i < count; i++) {
        const char* city = ds_list_get(cities, i);
        ds_list_t* group = ds_map_get(groups, city);
        cleanup_persons_list(group);
    }

    ds_list_free(cities);
    ds_map_free(groups);
}

/* ==================== Main Pipeline ==================== */

int main(void) {
    printf("========================================\n");
    printf("CosmoRun Data Pipeline Example\n");
    printf("========================================\n\n");

    // Step 1: Extract - Load data
    ds_list_t* all_persons = load_sample_data();
    if (!all_persons || ds_list_size(all_persons) == 0) {
        fprintf(stderr, "[ERROR] No data to process\n");
        return 1;
    }

    // Step 2: Transform - Filter adults (age >= 18)
    ds_list_t* adults = filter_by_age(all_persons, 18);

    // Step 3: Transform - Group by city
    ds_map_t* city_groups = group_by_city(adults);

    // Step 4: Analyze - Aggregate statistics
    print_aggregation(city_groups);

    // Step 5: Load - Generate outputs
    print_summary(adults, city_groups);
    print_tax_distribution(adults);
    export_to_csv(adults, "output.csv");

    // Cleanup
    cleanup_persons_list(all_persons);
    cleanup_persons_list(adults);
    cleanup_groups_map(city_groups);

    printf("\n✓ Pipeline completed successfully!\n");
    printf("  Demonstrated: Extract -> Transform -> Load (ETL)\n");
    printf("  Data structures used: List, Map\n");
    printf("  Operations: Filter, Group, Aggregate, Export\n");

    return 0;
}

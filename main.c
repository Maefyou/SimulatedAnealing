#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <windows.h>
#include <math.h>

#define MAX_NODES 1000 // Maximale Knotenzahl für Speicherreservierung

// Der Dateiname wird jetzt über die Kommandozeile übergeben
char FILE_NAME[MAX_NODES];

char namen[MAX_NODES][16]; // Knotennamen-Array
double node_time_sum = 0;  // Summe der Knotengewichte

// Globale Variablen für Knotenzahl und Kantenzahl
int node_count = 0;
int edge_count = 0;

// Hilfsfunktion: Wandelt Knotennamen in Index um
int name_to_idx(const char *name)
{
    for (int i = 0; i < node_count; ++i)
        if (strcmp(namen[i], name) == 0)
            return i;
    return -1;
}

// Liest eine GraphML-Datei ein, zählt Knoten/Kanten, füllt Matrix und Knotennamen
void graphml_to_adjacency_matrix(const char *filename, double ***matrix, int *nodes, int *edges)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Datei konnte nicht geöffnet werden");
        exit(1);
    }

    // Initialisieren
    for (int i = 0; i < MAX_NODES; ++i)
    {
        namen[i][0] = '\0';
    }
    node_time_sum = 0;
    *nodes = 0;
    *edges = 0;

    char line[256];
    char node_id[16];

    // Zähle Knoten und Kanten und speichere Namen
    while (fgets(line, sizeof(line), file))
    {
        // Knoten extrahieren
        if (strstr(line, " <node id=\""))
        {
            if (sscanf(line, " <node id=\"%15[^\"]", node_id) == 1)
            {
                strncpy(namen[*nodes], node_id, 15);
                namen[*nodes][15] = '\0';
                int idx = *nodes;
                (*nodes)++;
                double temp = 0.0;
                // Suche nach <data key="d0"> (Knotengewicht)
                if (fgets(line, sizeof(line), file))
                {
                    char *node_data_ptr = strstr(line, ">");
                    if (node_data_ptr)
                    {
                        if (sscanf(node_data_ptr + 1, "%lf", &temp) == 1)
                        {
                            node_time_sum += temp;
                        }
                    }
                }
            }
            continue;
        }
        // Kanten extrahieren
        if (strstr(line, " <edge "))
        {
            (*edges)++;
        }
    }
    fclose(file);

    // Matrix dynamisch allokieren
    *matrix = (double **)malloc((*nodes) * sizeof(double *));
    for (int i = 0; i < *nodes; ++i)
    {
        (*matrix)[i] = (double *)calloc(*nodes, sizeof(double));
    }

    // Matrix befüllen (zweiter Durchlauf)
    file = fopen(filename, "r");
    if (!file)
    {
        perror("Datei konnte nicht geöffnet werden");
        exit(1);
    }
    int node_idx = 0;
    while (fgets(line, sizeof(line), file))
    {
        // Knoten überspringen (bereits verarbeitet)
        if (strstr(line, " <node id=\""))
        {
            fgets(line, sizeof(line), file); // Knotengewicht überspringen
            continue;
        }
        // Kanten extrahieren
        char source_id[16], target_id[16];
        if (strstr(line, " <edge "))
        {
            if (sscanf(line, " <edge id=\"%*[^\"]\" source=\"%15[^\"]\" target=\"%15[^\"]", source_id, target_id) == 2)
            {
                int i = name_to_idx(source_id);
                int j = name_to_idx(target_id);
                double temp = 1.0;
                if (fgets(line, sizeof(line), file))
                {
                    char *edge_data_ptr = strstr(line, ">");
                    if (edge_data_ptr)
                    {
                        if (sscanf(edge_data_ptr + 1, "%lf", &temp) != 1)
                        {
                            temp = 1.0;
                        }
                    }
                }
                if (i >= 0 && i < *nodes && j >= 0 && j < *nodes)
                    (*matrix)[i][j] = temp;
            }
            continue;
        }
    }
    fclose(file);
}

// Berechnet die Kosten eines Pfads, inkl. Strafe für illegale Übergänge
double calculate_cost(int *path, int size, double **FromTo)
{
    double cost = 0.0;
    for (int i = 0; i < size - 1; i++)
    {
        int from = path[i];
        int to = path[i + 1];
        if (FromTo[from][to] == 0.0)
        {
            cost += 1000; // Strafe für illegale Kante
            continue;
        }
        cost += FromTo[from][to];
    }
    return cost + node_time_sum; // Knotengewichte addieren
}

// Vertauscht zufällig zwei Elemente im Pfad
void random_swap(int *path, int size, unsigned int seed)
{
    srand(seed);
    int i = rand() % size;
    int j = rand() % size;
    while (i == j) // Sicherstellen, dass i und j unterschiedlich sind
    {
        j = rand() % size;
    }
    int temp = path[i];
    path[i] = path[j];
    path[j] = temp;
}

// Versucht, einen Pfad möglichst gültig zu machen (weniger illegale Übergänge)
void maximise_path(int *path, int nodes, double **FromTo)
{
    int best_cost = calculate_cost(path, nodes, FromTo);
    int new_cost = best_cost;
    int working = 0;
    for (int iterations = 0; iterations < 2 * nodes; iterations++)
    {
        new_cost = calculate_cost(path, nodes, FromTo);
        if (new_cost < best_cost)
        {
            best_cost = new_cost;
        }
        // Prüfe auf ungültigen Übergang
        if (FromTo[path[(working - 1 + nodes) % nodes]][path[working]] == 0.0)
        {
            // Suche einen gültigen Knoten zum Tauschen
            for (int i = 0; i < nodes; i++)
            {
                if (FromTo[path[(i - 1 + nodes) % nodes]][path[i]] == 0.0 &&
                    FromTo[path[(working - 1 + nodes) % nodes]][path[i]] != 0.0)
                {
                    int temp = path[working];
                    path[working] = path[i];
                    path[i] = temp;
                    break;
                }
            }
        }
        working = (working + 1) % nodes;
    }
}

// Gibt die Anzahl gültiger Übergänge im Pfad zurück
int get_valid_transition_count(int *path, int size, double **FromTo)
{
    int valid_transitions = 0;
    for (int i = 0; i < size; i++)
    {
        if (FromTo[path[i]][path[(i + 1) % size]] != 0.0)
        {
            valid_transitions++;
        }
    }
    return valid_transitions;
}

// Simulated Annealing Algorithmus zur Optimierung des Pfads
void simulated_annealing(int *path, int size, double initial_temp, double cooling_rate, int max_iterations, unsigned int seed, double **FromTo)
{
    double temp = initial_temp;
    double best_cost = calculate_cost(path, size, FromTo);
    int *best_path = malloc(size * sizeof(int));
    memcpy(best_path, path, size * sizeof(int));
    int updates = 0;
    int *new_path = malloc(size * sizeof(int));
    memcpy(new_path, path, size * sizeof(int));
    int random_swaps = pow(size, 0.35); // Anzahl der möglichen Vertauschungen

    clock_t start_time = clock(); // Startzeit für ETA

    for (int iter = 0; iter < max_iterations; iter++)
    {
        for (int i = 0; i < random_swaps; i++)
        {
            random_swap(new_path, size, seed + iter + i);
        }

        maximise_path(new_path, size, FromTo); // Versuche, Pfad gültig zu machen

        double new_cost = calculate_cost(new_path, size, FromTo);
        srand(seed + iter); // Seed für Zufall
        // Akzeptiere neuen Pfad, wenn besser oder mit Wahrscheinlichkeit abhängig von Temperatur
        if ((new_cost < best_cost) ||
            (exp((best_cost - new_cost))*temp > (double)rand() / RAND_MAX))
        {
            updates++;
            memcpy(path, new_path, size * sizeof(int));
            best_cost = new_cost;
            memcpy(best_path, new_path, size * sizeof(int));
        }
        // Ausgabe alle 10000 Iterationen mit ETA
        if ((iter + 1) % 10000 == 0)
        {
            clock_t now = clock();
            double elapsed = (double)(now - start_time) / CLOCKS_PER_SEC;
            double eta = (max_iterations - iter - 1) * (elapsed / (iter + 1));
            printf("Iteration %d, Aktuelle Kosten: %lf mit %d validen kanten, ETA: %.1f Sekunden\n", iter + 1, best_cost, get_valid_transition_count(best_path, size, FromTo),eta);
            printf("  Aktueller bester Pfad: ");
            for (int k = 0; k < size; k++)
                printf("%s ", namen[best_path[k]]);
            printf("\n");
            printf("\n");
        }
        temp = temp * cooling_rate; // Temperatur verringern
    }
    printf("Best cost: %lf\n", best_cost);
    printf("Best path: ");
    for (int i = 0; i < size; i++)
    {
        printf("%s ", namen[best_path[i]]);
    }
    printf("Updates: %d\n", updates);
    free(best_path);
    free(new_path);
}

// Erzeugt einen zufälligen Pfad (Permutation der Knoten)
void generate_random_path(int *path, int size, unsigned int seed)
{
    srand(seed);
    for (int i = 0; i < size; i++)
    {
        path[i] = i;
    }
    for (int i = size - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int temp = path[i];
        path[i] = path[j];
        path[j] = temp;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <graphml_file>\n", argv[0]);
        return 1;
    }
    strncpy(FILE_NAME, argv[1], MAX_NODES - 1);
    FILE_NAME[MAX_NODES - 1] = '\0';

    double **FromTo = NULL;
    // Graph einlesen, Knotenzahl und Kantenzahl bestimmen, Matrix allokieren
    graphml_to_adjacency_matrix(FILE_NAME, &FromTo, &node_count, &edge_count);

    printf("Erkannte Knoten: %d\n", node_count);
    printf("Erkannte Kanten: %d\n", edge_count);

    // Ausgabe der Adjazenzmatrix
    printf("   ");
    for (int i = 0; i < node_count; i++)
        printf(" %s", namen[i]);
    printf("\n");
    for (int i = 0; i < node_count; i++)
    {
        printf("%s: ", namen[i]);
        for (int j = 0; j < node_count; j++)
        {
            if (FromTo[i][j] != 0.0)
                printf("%5.1f ", FromTo[i][j]);
            else
                printf("  -  ");
        }
        printf("\n");
    }

    printf("\nNode Time Sum: %lf\n", node_time_sum);

    // Parameter für Simulated Annealing
    double initial_temp = 100*node_count; // Starttemperatur
    double cooling_rate = 1 - (1 / pow(node_count, 2.5)); // Abkühlungsrate
    int max_iterations = 100000 * pow(node_count, 0.6); // Maximale Iterationen
    int *path = malloc(node_count * sizeof(int));

    // Führe Simulated Annealing mehrfach aus
    for (int i = 0; i < 1; i++)
    {
        generate_random_path(path, node_count, (unsigned int)time(NULL) + i);
        simulated_annealing(path, node_count, initial_temp, cooling_rate, max_iterations, (unsigned int)time(NULL), FromTo);
        // Prüfe Pfad auf gültige Übergänge und eindeutige Knoten
        int *visited = calloc(node_count, sizeof(int));
        int valids = 0;
        int unique_nodes = 0;
        for (int j = 0; j < node_count; j++)
        {
            if (FromTo[path[j]][path[(j + 1) % node_count]] != 0.0)
            {
                valids++;
            }
            if (visited[path[j]] == 0)
            {
                visited[path[j]] = 1;
                unique_nodes++;
            }
        }
        printf("Valid transitions: %d, Unique nodes: %d\n", valids, unique_nodes);
        int temp_path[MAX_NODES];
        memcpy(temp_path, path, node_count * sizeof(int));
        int rotated = 0;
        for(int j = 0; j < node_count; j++)
        {
            if(FromTo[temp_path[j]][temp_path[(j + 1) % node_count]] == 0.0)
            {
                rotated = 1;
                for(int k = 0; k < node_count; k++)
                {
                    path[k] = temp_path[(k + j + 1) % node_count];
                }
            }
            if(rotated)
            {
                break;
            }
        }

        free(visited);
    }

    // Ausgabe des finalen Pfads
    int illegals = 0;
    printf("Final path: ");
    for (int i = 0; i < node_count; i++)
    {
        printf("%s ", namen[path[i]]);
        if (FromTo[path[i]][path[(i + 1) % node_count]] == 0.0)
        {
            illegals++;
            printf("(illegal) ");
        }
    }
    printf("\n");
    printf("cost: %lf\n", calculate_cost(path, node_count, FromTo));
    printf("illegals in circle (-1 for path): %d\n", illegals);

    // Speicher freigeben
    for (int i = 0; i < node_count; ++i)
    {
        free(FromTo[i]);
    }
    free(FromTo);
    free(path);
    return 0;
}
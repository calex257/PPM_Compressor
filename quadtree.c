#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

//macro pentru a extrage o anumita culoare de la un index din buffer-ul
//in care este stocata imaginea
#define COLOR_AT(buffer, line, column, width, type)\
    (*(uint8_t *)((buffer) + 3 * (line) * (width) + (column)*3 + (type)))

//structura din cerinta
typedef struct QuadtreeNode
{
    unsigned char blue, green, red;
    uint32_t area;
    int32_t top_left, top_right;
    int32_t bottom_left, bottom_right;
} __attribute__((packed)) QuadtreeNode;

//enum pentru a asocia culorilor un numar pentru lizibilitate
enum colors
{
    RED = 0,
    GREEN = 1,
    BLUE = 2,
    ALL_COLORS = 3
};

//enum pentru a asocia celor 4 zone din matrice un numar pentru lizibilitate
enum tree_children
{
    TOP_LEFT = 0,
    TOP_RIGHT = 1,
    BOT_RIGHT = 2,
    BOT_LEFT = 3,
    ALL_CHILDREN = 4
};

//struct pentru a retine informatiile necesare pentru parcurgerea
//unei subzone din matrice. este nevoie de coordonata initiala(xstart, ystart),
//dimensiunea subzonei(sqrsize) si latimea pozei(pentru a determina corect
//indexul din buffer)
typedef struct square
{
    int xstart, ystart, sqrsize, photowidth;
} square;

//macro cu rol asemanator unui constructor din OOP, initializeaza o variabile square
//cu valorile transmise ca parametri
#define SQUARE(xst, yst, sz, phw)\
    (square) { .xstart = (xst), .ystart = (yst), .sqrsize = (sz), .photowidth = (phw) }

//structura pentru realizarea arborelui cuaternar
typedef struct qnode
{
    //sunt necesari 4 pointeri pentru fii nodului
    //indexul fiecarui copil corespunde unei subzone conform enum-ului tree_children
    struct qnode* children[4];

    //retine latura patratului care reprezinta subzona
    int size;

    //culoarea medie a subzonei
    uint8_t avg_r, avg_g, avg_b;
} qnode;

//calculeaza media aritmetica a culorilor dintr-o anumita zona(sqr)
uint8_t* get_mean(uint8_t* data, square* sqr)
{
    uint8_t* aux = malloc(sizeof(uint8_t) * 3);

    //se folosesc unsigned int pe 64 de biti pentru a preveni overflow-ul
    //indexul din sum corespunde culorii din enum-ul colors
    uint64_t sum[3] = { 0, 0, 0 };

    //se parcurg toti pixelii din zona matricii
    for (int i = sqr->xstart; i < sqr->xstart + sqr->sqrsize; i++) {
        for (int j = sqr->ystart; j < sqr->ystart + sqr->sqrsize; j++) {

            //pentru fiecare culoare se calculeaza suma valorilor din zona
            for (int k = RED; k < ALL_COLORS; k++) {
                sum[k] += COLOR_AT(data, i, j, sqr->photowidth, k);
            }
        }
    }

    //fiecare suma calculata se imparte la numarul de pixeli care
    //este latura patratului la puterea a 2-a
    for (int i = RED; i < ALL_COLORS; i++) {
        aux[i] = sum[i] / (sqr->sqrsize * sqr->sqrsize);
    }

    //se returneaza vectorul de medii
    return aux;
}

//calculeaza scorul de similarite pentru o zona data
uint64_t get_sim_score(uint8_t* data, square* sqr, uint8_t* avg)
{
    uint64_t sum = 0;
    int diff;

    //se parcurg toti pixelii din zona
    for (int i = sqr->xstart; i < sqr->xstart + sqr->sqrsize; i++) {
        for (int j = sqr->ystart; j < sqr->ystart + sqr->sqrsize; j++) {

            //pentru fiecare culoare se calculeaza abaterea
            for (int k = RED;k < ALL_COLORS;k++) {
                diff = avg[k] - COLOR_AT(data, i, j, sqr->photowidth, k);

                //se adauga la suma patratul fiecarei abateri
                sum += diff * diff;
            }
        }
    }

    //se imparte rezultatul la 3*numarul de pixeli
    sum = sum / (3 * sqr->sqrsize * sqr->sqrsize);
    free(avg);
    return sum;
}

//citeste matricea de pixeli dintr-un fisier .ppm
//returneaza buffer-ul in care este stocata
uint8_t* file_to_photo(char* filename, int* size)
{
    //buffer-ul este de tip unsigned char si va fi alocat dinamic
    uint8_t* data;
    char buf[50];
    FILE* input = fopen(filename, "rb");

    //se citeste si se ignora prima linie, care contine sirul P6
    fgets(buf, 50, input);

    //se citeste a doua linie din care se extrage dimensiunea pozei
    //se garanteaza ca imaginea este patrat deci latimea=inaltimea
    fgets(buf, 50, input);
    sscanf(buf, "%d", size);

    //se citeste si se ignora ultima linie
    fgets(buf, 50, input);

    //se aloca memorie pentru matricea de pixeli
    //numarul de pixeli*numarul de culori pentru un pixel*dimensiunea unui char
    data = malloc((*size) * (*size) * 3 * sizeof(unsigned char));

    //se citeste matricea de pixeli si se inchide fisierul
    fread(data, sizeof(unsigned char), (*size) * (*size) * 3, input);
    fclose(input);
    return data;
}

//aloca memorie pentru un nod nou si initializeaza toti copiii lui cu NULL
//si returneaza nodul creat
qnode* new_node()
{
    qnode* qn = malloc(sizeof(qnode));
    for (int i = TOP_LEFT; i < ALL_CHILDREN; i++) {
        qn->children[i] = NULL;
    }
    return qn;
}

//creeaza arborele cuaternar pe baza imaginii
//functia creeaza cate un nod nou pentru fiecare zona si apoi se apeleaza recursiv
//pentru copiii nodului si subzonele din zona initiala
void photo_to_tree(qnode** root, uint8_t* data, int64_t factor, uint32_t* nr_nodes, square* sqr)
{
    //se creeaza un nod nou si se incrementeaza numarul de noduri
    if (*root == NULL) {
        *root = new_node();
        (*nr_nodes)++;
    }

    //se calculeaza culoarea medie si se stocheaza in nodul nou odata cu dimensiunea
    uint8_t* mn = get_mean(data, sqr);
    (*root)->avg_r = mn[RED];
    (*root)->avg_g = mn[GREEN];
    (*root)->avg_b = mn[BLUE];
    (*root)->size = sqr->sqrsize;

    //se calculeaza scorul de similaritate pentru zona
    int64_t score = get_sim_score(data, sqr, mn);

    //daca acesta este mai mic decat factorul de compresie
    //zona nu se mai imparte in 4
    if (score <= factor) {
        return;
    }

    //in caz contrar se calculeaza pozitiile de inceput pentru fiecare subzona
    //si se apeleaza functia pentru copiii nodului curent cu subzona corespunzatoare
    //fiecare patrat are latura sqr->sqrsize/2, unde sqr->sqrsize este latura
    //patratului initial
    square subsquare = SQUARE(sqr->xstart, sqr->ystart, sqr->sqrsize / 2, sqr->photowidth);
    photo_to_tree(&(*root)->children[TOP_LEFT], data, factor, nr_nodes, &subsquare);

    //al doilea patrat incepe la xstart si ystart+size/2
    subsquare.ystart = sqr->ystart + sqr->sqrsize / 2;
    photo_to_tree(&(*root)->children[TOP_RIGHT], data, factor, nr_nodes, &subsquare);


    //al treilea patrat incepe la xstart+size/2 si ystart+size/2
    subsquare.xstart = sqr->xstart + sqr->sqrsize / 2;
    photo_to_tree(&(*root)->children[BOT_RIGHT], data, factor, nr_nodes, &subsquare);


    //al patrulea patrat incepe la xstart+size/2 si ystart
    subsquare.ystart = sqr->ystart;
    photo_to_tree(&(*root)->children[BOT_LEFT], data, factor, nr_nodes, &subsquare);
}

//functie care verifica daca un nod este frunza
//returneaza 1 daca nodul e frunza si 0 in caz contrar
int is_leaf(qnode* tree)
{
    //se parcurg toate nodurile si se verifica daca sunt nule
    for (int i = TOP_LEFT;i < ALL_CHILDREN;i++) {
        if (tree->children[i] != NULL) {
            return 0;
        }
    }
    return 1;
}

//functie care creeaza vectorul pe baza arborelui si calculeaza numarul de frunze
void tree_to_arr(QuadtreeNode* arr, qnode* root, uint32_t* nr_leaves, uint32_t* len)
{
    if (root == NULL) {
        return;
    }

    //se trec informatiile din nod in elementul din vector de la indexul curent
    arr[(*len)].area = root->size * root->size;
    arr[(*len)].blue = root->avg_b;
    arr[(*len)].red = root->avg_r;
    arr[(*len)].green = root->avg_g;

    //se retine indexul si se incrementeaza lungimea
    int curr_index = (*len);
    (*len)++;

    //daca nodul este frunza se incrementeaza numarul de frunze
    if (is_leaf(root)) {
        (*nr_leaves)++;

        //si se atribuie valoarea -1 indecsilor descendentilor
        //deoarece pentru structura s-a folosit __attribute__((packed))
        //am iterat prin indecsi ca si cum ar fi un vector, nefiind padding
        //intre ei si avand aceeasi dimensiune si acelasi tip
        for (int i = 0;i < 4;i++) {
            *((&arr[curr_index].top_left) + i) = -1;
        }
        return;
    }

    //nodul din pozitia top_left se pozitioneaza imediat dupa cel curent
    arr[curr_index].top_left = (*len);

    //se repeta procedeul pentru subarborele top left si astfel calculeaza
    //indexul pentru top right
    tree_to_arr(arr, root->children[TOP_LEFT], nr_leaves, len);
    arr[curr_index].top_right = (*len);

    //idem pentru bottom right si bottom left
    tree_to_arr(arr, root->children[TOP_RIGHT], nr_leaves, len);
    arr[curr_index].bottom_right = (*len);
    tree_to_arr(arr, root->children[BOT_RIGHT], nr_leaves, len);
    arr[curr_index].bottom_left = (*len);
    tree_to_arr(arr, root->children[BOT_LEFT], nr_leaves, len);
}

//elibereaza recursiv memoria aferenta unui arbore
//se realizeaza o parcurgere in postordine
void free_tree(qnode* root)
{
    if (root == NULL) {
        return;
    }
    for (int i = TOP_LEFT;i < ALL_CHILDREN;i++) {
        free_tree(root->children[i]);
    }
    free(root);
}

//calculeaza radicalul unui numar putere para a lui 2 pe maxim 32 de biti
int fast_sqrt(int n)
{
    for (int i = 0;i < 16;i++) {

        //stiind ca un numar putere a lui 2 in binar
        //are un singur 1 si 0 in rest, se parcurg toate numerele
        //puteri pare ale lui 2 pana cand se identifica n
        //daca n este 2^(2*i) radical din n este 2^i
        if ((1 << (2 * i)) == n) {
            return 1 << i;
        }
    }
    return -1;
}

//realizeaza conversia din element din vector in nod in arbore pentru un element
//si apoi recursiv pentru descendentii lui
void arr_to_tree(qnode** root, QuadtreeNode* arr, int index)
{
    //daca indexul este negativ inseamna ca nodul parinte este o frunza
    //si nu se face nimic
    if (index == -1) {
        return;
    }

    //se creeaza un nod nou in care se pun valorile elementului din vector
    if ((*root) == NULL) {
        (*root) = new_node();
    }
    (*root)->avg_r = arr[index].red;
    (*root)->avg_g = arr[index].green;
    (*root)->avg_b = arr[index].blue;
    (*root)->size = fast_sqrt(arr[index].area);

    //se repeta pentru descendentii nodului
    arr_to_tree(&((*root)->children[TOP_LEFT]), arr, arr[index].top_left);
    arr_to_tree(&((*root)->children[TOP_RIGHT]), arr, arr[index].top_right);
    arr_to_tree(&((*root)->children[BOT_RIGHT]), arr, arr[index].bottom_right);
    arr_to_tree(&((*root)->children[BOT_LEFT]), arr, arr[index].bottom_left);
}

//functie care reconstruieste imaginea pornind de la arborele cuaternar
void tree_to_photo(uint8_t* data, qnode* tree, square* sqr)
{
    if (tree == NULL) {
        return;
    }

    //daca nodul este frunza inseamna ca informatia lui este utila
    //pentru reconstruirea pozei
    if (is_leaf(tree)) {

        //se parcurge fiecare pixel din zona asociata nodului
        //si i se atribuie culoarea media stocata in nod
        for (int i = sqr->xstart;i < sqr->xstart + sqr->sqrsize;i++) {
            for (int j = sqr->ystart;j < sqr->ystart + sqr->sqrsize;j++) {
                COLOR_AT(data, i, j, sqr->photowidth, RED) = tree->avg_r;
                COLOR_AT(data, i, j, sqr->photowidth, GREEN) = tree->avg_g;
                COLOR_AT(data, i, j, sqr->photowidth, BLUE) = tree->avg_b;
            }
        }
        return;
    }

    //daca nodul nu este frunza, se parcurge arborele in adancime
    //modificandu-se zona din imagine corespunzator fiecarui fiu
    square subsquare = SQUARE(sqr->xstart, sqr->ystart, sqr->sqrsize / 2, sqr->photowidth);
    tree_to_photo(data, tree->children[TOP_LEFT], &subsquare);
    subsquare.ystart = sqr->ystart + sqr->sqrsize / 2;
    tree_to_photo(data, tree->children[TOP_RIGHT], &subsquare);
    subsquare.xstart = sqr->xstart + sqr->sqrsize / 2;
    tree_to_photo(data, tree->children[BOT_RIGHT], &subsquare);
    subsquare.ystart = sqr->ystart;
    tree_to_photo(data, tree->children[BOT_LEFT], &subsquare);
}

//se citesc din fisierul de input numarul de noduri, numarul de culori si vectorul
//si se returneaza vectorul
QuadtreeNode* file_to_arr(char* file, uint32_t* nr_colors, uint32_t* nr_nodes)
{
    FILE* input = fopen(file, "rb");
    fread(nr_colors, sizeof(uint32_t), 1, input);
    fread(nr_nodes, sizeof(uint32_t), 1, input);
    QuadtreeNode* arr = malloc(sizeof(QuadtreeNode) * (*nr_nodes));
    fread(arr, sizeof(QuadtreeNode), *nr_nodes, input);
    fclose(input);
    return arr;
}

//functie care oglindeste un arbore pe vericala
void mirror_v(qnode* tree)
{
    if (tree == NULL) {
        return;
    }

    //se parcurge in postordine
    for (int i = TOP_LEFT;i < ALL_CHILDREN;i++) {
        mirror_v(tree->children[i]);
    }
    qnode* aux;

    //se schimba intre ele nodurile top left si bottom left
    aux = tree->children[TOP_LEFT];
    tree->children[TOP_LEFT] = tree->children[BOT_LEFT];
    tree->children[BOT_LEFT] = aux;

    //respectiv top right si bottom right
    aux = tree->children[TOP_RIGHT];
    tree->children[TOP_RIGHT] = tree->children[BOT_RIGHT];
    tree->children[BOT_RIGHT] = aux;
}

//functie care oglindeste un arbore pe orizontala
void mirror_h(qnode* tree)
{
    if (tree == NULL) {
        return;
    }

    //se parcurge in postordine
    for (int i = TOP_LEFT;i < ALL_CHILDREN;i++) {
        mirror_h(tree->children[i]);
    }
    qnode* aux;

    //se schimba intre ele nodurile top left si top right
    aux = tree->children[TOP_LEFT];
    tree->children[TOP_LEFT] = tree->children[TOP_RIGHT];
    tree->children[TOP_RIGHT] = aux;

    //respectiv bottom left si bottom right
    aux = tree->children[BOT_LEFT];
    tree->children[BOT_LEFT] = tree->children[BOT_RIGHT];
    tree->children[BOT_RIGHT] = aux;
}

//se scrie in fisierul de iesire vectorul de QuadtreeNode-uri
//si numarul de noduri respectiv frunze
void arr_to_file(QuadtreeNode* arr, char* filename, int32_t nr_leaves, int32_t nr_nodes)
{
    FILE* output = fopen(filename, "wb");
    fwrite(&nr_leaves, sizeof(uint32_t), 1, output);
    fwrite(&nr_nodes, sizeof(uint32_t), 1, output);
    fwrite(arr, sizeof(QuadtreeNode), nr_nodes, output);
    fclose(output);
}

//se scrie in fisierul de output imaginea rezultata
void photo_to_file(uint8_t* photo, char* filename, uint32_t size)
{
    FILE* output = fopen(filename, "wb");
    char info[30];

    //se scrie mai intai antetul
    sprintf(info, "P6\n%d %d\n255\n", size, size);
    fwrite(info, sizeof(char), strlen(info), output);

    //apoi se scrie matricea de pixeli care reprezinta imaginea propriu-zisa
    fwrite(photo, sizeof(uint8_t), 3 * size * size, output);
    fclose(output);
}

//functie care realizeaza operatia de compresie
void compress(int argc, char** argv)
{
    int size;

    //se citeste matricea de pixeli din fisierul de intrare
    uint8_t* data = file_to_photo(argv[argc - 2], &size);
    qnode* tree = NULL;

    //se extrage factorul din argumente
    uint32_t nr_nodes = 0, len = 0, nr_leaves = 0, fact = atoi(argv[2]);
    square sqr = SQUARE(0, 0, size, size);

    //se creeaza arborele pe baza matricii de pixeli
    photo_to_tree(&tree, data, fact, &nr_nodes, &sqr);

    //se creeaza vectorul pe baza arborelui
    QuadtreeNode* arr = malloc(nr_nodes * sizeof(QuadtreeNode));
    tree_to_arr(arr, tree, &nr_leaves, &len);

    //se scrie vectorul in fisierul de output
    arr_to_file(arr, argv[argc - 1], nr_leaves, nr_nodes);
    free_tree(tree);
    free(data);
    free(arr);
}

//functie care realizeaza operatia de decompresie
void decompress(int argc, char** argv)
{
    uint32_t nr_colors, nr_nodes;

    //se citeste vectorul din fisierul de input
    QuadtreeNode* arr = file_to_arr(argv[argc - 2], &nr_colors, &nr_nodes);
    qnode* tree = NULL;

    //se realizeaza arborele pe baza vectorului
    arr_to_tree(&tree, arr, 0);

    //se reconstruieste imaginea pe baza arborelui
    uint8_t* photo = malloc(sizeof(uint8_t) * tree->size * tree->size * 3);
    square sqr = SQUARE(0, 0, tree->size, tree->size);
    tree_to_photo(photo, tree, &sqr);

    //se scrie imaginea in fisierul de iesire
    photo_to_file(photo, argv[argc - 1], tree->size);
    free_tree(tree);
    free(photo);
    free(arr);
}

//functie care realizeaza operatia de mirror
void mirror(int argc, char** argv)
{
    //se extrage factorul de compresie din argumente
    uint64_t fact = atoi(argv[3]);
    int size;

    //se realizeaza matricea de pixeli pe baza imaginii
    uint8_t* data = file_to_photo(argv[argc - 2], &size);
    uint32_t nr_nodes = 0;
    qnode* tree = NULL;
    square sqr = SQUARE(0, 0, size, size);

    //se realizeaza arborele pe baza matricii de pixeli
    photo_to_tree(&tree, data, fact, &nr_nodes, &sqr);

    //se realizeaza oglindirea de tipul cerut
    if (strcmp(argv[2], "h") == 0) {
        mirror_h(tree);
    }
    else if (strcmp(argv[2], "v") == 0) {
        mirror_v(tree);
    }
    else {
        fprintf(stderr, "invalid mirror type");
    }

    //se reconstruieste imaginea din arborele oglindit
    tree_to_photo(data, tree, &sqr);

    //se creeaza fisierul de output cu imaginea noua
    photo_to_file(data, argv[argc - 1], tree->size);
    free_tree(tree);
    free(data);
}

int main(int argc, char** argv)
{
    
    //se determina tipul operatiei care trebuie efectuata
    //si se apeleaza functia corespunzatoare ei
    if (strcmp(argv[1], "-c") == 0) {
        compress(argc, argv);
    }
    else if (strcmp(argv[1], "-d") == 0) {
        decompress(argc, argv);
    }
    else if (strcmp(argv[1], "-m") == 0) {
        mirror(argc, argv);
    }
    else {
        fprintf(stderr, "invalid argument\n");
    }
    return 0;
}
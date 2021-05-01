/*
PRL projekt 2 - Mesh multiplication
autor: Havlicek Lukas (xhavli46)
*/

#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <chrono>

#define TAG 0
#define TAG_A 1
#define TAG_B 2

std::chrono::high_resolution_clock::time_point start,end;//pro mereni casu

//funkce co nacte matici ze souboru(dle argumentu) a ulozi ji do vectoru
std::vector<std::vector<int>> get_mat(const char *filename)
{
    std::vector<std::vector<int>> mat;
    std::ifstream file(filename);

    std::string line;

    std::getline(file, line);
    std::stringstream lineStream(line);
    int mat_rows_cols = 0;
    lineStream >> mat_rows_cols; //ziskani prvniho radku, tj poctu radku v mat1

    while (std::getline(file, line))
    {
        std::vector<int> mat_line;
        int x;
        std::stringstream lineStream(line);
        while (lineStream >> x)
        {
            mat_line.push_back(x);
        }
        mat.push_back(mat_line);
    }
    std::vector<int> mat_line;
    mat_line.push_back(mat_rows_cols);
    mat.push_back(mat_line); //prvni radek souboru je na konci vectoru

    return mat;
}

//funkce kontrolujici matici ve vectoru (vsechny radky musi mit stejny pocet hodnot)
void check_mat(std::vector<std::vector<int>> mat)
{
    int test = mat.front().size();
    for (auto i = mat.begin(); i != mat.end(); i++)
    {
        if (test != (*i).size())
        {
            //jeden radek matice ma jiny pocet nez ostatni
            MPI_Abort(MPI_COMM_WORLD, MPI_ERR_COUNT);
            return;
        }
        test = (*i).size();
    }
}

//funkce, kterou provadi master procesor na zacatku
void master(int *tmp_arr)
{
    auto mat1 = get_mat("mat1");
    auto mat2 = get_mat("mat2");

    //prvni prvek fronty je pocet radku resp. sloupcu
    int mat1_rows = mat1.back().back();
    int mat2_cols = mat2.back().back();
    mat1.pop_back();
    mat2.pop_back();

    //kontrola zda vsechny radky maji stejny pocet hodnot
    check_mat(mat1);
    check_mat(mat2);

    int mat1_cols = mat1.front().size();
    int mat2_rows = mat2.size();

    if (mat1_cols != mat2_rows)
    { //pokud pocet sloupcu matice A neni shodny s poctem radku matice B, nelze je nasobit
        MPI_Abort(MPI_COMM_WORLD, MPI_ERR_COUNT);
        return;
    }

    tmp_arr[0] = mat1_cols;
    tmp_arr[1] = mat1_rows;
    tmp_arr[2] = mat2_cols;

    start = std::chrono::high_resolution_clock::now();

    //rozeslani hodnot potrebnych v algoritmu vsem procesorum
    MPI_Bcast(tmp_arr, 3, MPI_INT, 0, MPI_COMM_WORLD);

    int id = 0;
    for (auto i = mat1.begin(); i != mat1.end(); i++)
    { //rozeslani matice A
        for (auto j = (*i).begin(); j != (*i).end(); j++)
        {
            int x = *j;
            MPI_Send(&x, 1, MPI_INT, id, TAG_A, MPI_COMM_WORLD); //zasleme procesorum hodnoty
        }
        id += mat2_cols;
    }

    id = 0;
    for (auto i = mat2.begin(); i != mat2.end(); i++)
    { //rozeslani matice B
        for (auto j = (*i).begin(); j != (*i).end(); j++)
        {
            int x = *j;
            MPI_Send(&x, 1, MPI_INT, id, TAG_B, MPI_COMM_WORLD); //zasleme procesorum hodnoty
            id += 1;
        }
        id = 0;
    }
    return;
}

//funkce kterou provadi kazdy procesor (provedeni algoritmu)
void every_proc(int myid, int size, int out_rows, int out_cols)
{
    MPI_Status stat;
    int pos_i = myid / out_cols;
    int pos_j = myid % out_cols;
    int out = 0;
    int recv_a_id = 0;
    int recv_b_id = 0;
    if (pos_i > 0)
    { //procesor je v jinem nez prvnim radku matice
        recv_b_id = myid - out_cols;
    }

    if (pos_j != 0)
    { //procesor je v jinem nez prvnim sloupci
        recv_a_id = myid - 1;
    }

    for (int i = 0; i < size; i++)
    {
        int a = 0;
        int b = 0;

        MPI_Recv(&a, 1, MPI_UNSIGNED, recv_a_id, TAG_A, MPI_COMM_WORLD, &stat); //prijmeme od predchoziho procesoru
        MPI_Recv(&b, 1, MPI_UNSIGNED, recv_b_id, TAG_B, MPI_COMM_WORLD, &stat); //prijmeme od predchoziho procesoru

        out += a * b;

        if (pos_i < out_rows - 1)
            MPI_Send(&b, 1, MPI_INT, myid + out_cols, TAG_B, MPI_COMM_WORLD);
        if (pos_j < out_cols - 1)
            MPI_Send(&a, 1, MPI_INT, myid + 1, TAG_A, MPI_COMM_WORLD);
    }
    MPI_Send(&out, 1, MPI_INT, 0, TAG, MPI_COMM_WORLD); //zasleme zpet masteru hodnoty pro tisk
}

void print_mat(int numprocs, int out_rows, int out_cols)
{
    //vytisk casu z mereni
    end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //std::cout << "time: " << duration.count() << " us" << std::endl;


    //vytisk matice
    printf("%d:%d\n", out_rows, out_cols);
    MPI_Status stat;
    int cnt = 0;
    for (int i = 0; i < numprocs; i++)
    {
        cnt++;
        int x;
        MPI_Recv(&x, 1, MPI_UNSIGNED, i, TAG, MPI_COMM_WORLD, &stat); //prijmeme od vsech procesoru vysledky
        if (cnt >= out_cols)
        {
            cnt = 0;
            printf("%d\n", x);
        }
        else
        {
            printf("%d ", x);
        }
    }
}

int main(int argc, char *argv[])
{
    int numprocs;
    int myid;
    int i;
    MPI_Status stat;

    MPI_Init(&argc, &argv);                   //init
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs); //pocet procesu
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);     //id sveho procesu

    if (myid == 0)
    {
        int tmp_arr[3];
        master(tmp_arr);
        every_proc(myid, tmp_arr[0], tmp_arr[1], tmp_arr[2]);
        print_mat(numprocs, tmp_arr[1], tmp_arr[2]);
    }
    else
    {
        int tmp_arr[3];
        MPI_Bcast(tmp_arr, 3, MPI_INT, 0, MPI_COMM_WORLD);
        every_proc(myid, tmp_arr[0], tmp_arr[1], tmp_arr[2]);
    }

    MPI_Finalize();
    return 0;
}

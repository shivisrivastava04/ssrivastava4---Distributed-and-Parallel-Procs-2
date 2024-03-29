#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

typedef struct {float r; float i;} complex;
static complex ctmp;

#define currRank(a,b) {ctmp=(a);(a)=(b);(b)=ctmp;}

#define N 512


void fftd_1(complex *r, int n, int isign)
{
    int     m,i,i1,j,k,i2,l,l1,l2;
    float   c1,c2,z;
    complex t, u;

    if (isign == 0) return;

    /* Do the bit reversal */
    i2 = n >> 1;
    j = 0;
    for (i=0;i<n-1;i++) {
        if (i < j)
        currRank(r[i], r[j]);
        k = i2;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    /* m = (int) log2((double)n); */
    for (i=n,m=0; i>1; m++,i/=2);

    /* Compute the FFT */
    c1 = -1.0;
    c2 =  0.0;
    l2 =  1;
    for (l=0;l<m;l++) {
        l1   = l2;
        l2 <<= 1;
        u.r = 1.0;
        u.i = 0.0;
        for (j=0;j<l1;j++) {
            for (i=j;i<n;i+=l2) {
                i1 = i + l1;

                /* t = u * r[i1] */
                t.r = u.r * r[i1].r - u.i * r[i1].i;
                t.i = u.r * r[i1].i + u.i * r[i1].r;

                /* r[i1] = r[i] - t */
                r[i1].r = r[i].r - t.r;
                r[i1].i = r[i].i - t.i;

                /* r[i] = r[i] + t */
                r[i].r += t.r;
                r[i].i += t.i;
            }
            z =  u.r * c1 - u.i * c2;

            u.i = u.r * c2 + u.i * c1;
            u.r = z;
        }
        c2 = sqrt((1.0 - c1) / 2.0);
        if (isign == -1) /* FWD FFT */
        c2 = -c2;
        c1 = sqrt((1.0 + c1) / 2.0);
    }

    /* Scaling for inverse transform */
    if (isign == 1) {       /* IFFT*/
        for (i=0;i<n;i++) {
            r[i].r /= n;
            r[i].i /= n;
        }
    }
}

void getDataHere(char fileName[15], complex **data){
    FILE *fp = fopen(fileName, "r");

    int i, j, result;

    for (i=0;i<N;i++){
        for (j=0;j<N;j++){
            result = fscanf(fp,"%g",&data[i][j].r);
            data[i][j].i = 0.00;
        }
    }

    fclose(fp);
}

void transpose(complex **data, complex ** transp){
    int i, j;
    for (i = 0; i < N; i++)
      for(j = 0 ; j < N ; j++)
         transp[j][i] = data[i][j];
}

void functio_1(complex **data1, complex **data2, complex **data3){

    int i, j;

    float real, imag;

    for(i=0;i<N;i++){
        for(j=0;j<N;j++){
            data3[i][j].r = (data1[i][j].r * data2[i][j].r) - (data1[i][j].i * data2[i][j].i);
            data3[i][j].i = (data1[i][j].r * data2[i][j].i) + (data1[i][j].i * data2[i][j].r);
        }
    }
}

void printfile(char fileName[15], complex **data){

    FILE *fp = fopen(fileName, "w");

    int i, j;

    for (i=0;i<N;i++) {
        for (j=0;j<N;j++){
            fprintf(fp,"   %.7e",data[i][j].r);
        }
        fprintf(fp,"\n");
    }

    fclose(fp);
}

int main(int argc, char **argv){
    int currRank, p, source = 0, dest, x;

    complex **data1, **data2, **data3, **data4;
    data1 = malloc(N * sizeof(complex *));
    data2 = malloc(N * sizeof(complex *));
    data3 = malloc(N * sizeof(complex *));
    data4 = malloc(N * sizeof(complex *));
    for(x = 0; x < N; x++){
        data1[x] = malloc(N * sizeof(complex *));
        data2[x] = malloc(N * sizeof(complex *));
        data3[x] = malloc(N * sizeof(complex *));
        data4[x] = malloc(N * sizeof(complex *));
    }
    complex *vec;

    char fileName1[15] = "sample/2_im1";
    char fileName2[15] = "sample/2_im2";
    char fileName3[15] = "mpi_out_test2";

    MPI_Status status;

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &currRank);

    MPI_Comm_size(MPI_COMM_WORLD, &p);

    if(p != 8){
        printf("This program should run with 8 processors!\n");
        return 0;
    }

    /* Setup description of the 4 MPI_FLOAT fields x, y, z, velocity */
    MPI_Datatype mystruct;
    int          blocklens[2] = { 1, 1 };
    MPI_Aint     indices[2] = { 0, sizeof(float) };
    MPI_Datatype old_types[2] = { MPI_FLOAT, MPI_FLOAT };

    /* Make relative */
    MPI_Type_struct( 2, blocklens, indices, old_types, &mystruct );
    MPI_Type_commit( &mystruct );


    int i,j;

    double startTime, stopTime;

    //Starting and send rows of data1, data2

    int offset  = N/2;

    int tag = 345;

    if(currRank == 6){
        /* Start Clock */
        printf("\nStarting clock.\n");
        startTime = MPI_Wtime();
    }

    //Starting and send rows of data1, data2

    if(currRank == 0){
        getDataHere(fileName1, data1);

        for(j = offset; j < N; j++)
            MPI_Send(&data1[j][0], N, mystruct, 1, tag, MPI_COMM_WORLD);
    }

    if(currRank == 1){

        for(j = offset; j < N; j++)
            MPI_Recv(data1[j], N, mystruct, 0, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 2){
        getDataHere(fileName2, data2);

        for(j = offset; j < N; j++)
            MPI_Send(&data2[j][0], N, mystruct, 3, tag, MPI_COMM_WORLD);
    }

    if(currRank == 3){

        for(j = offset; j < N; j++)
            MPI_Recv(data2[j], N, mystruct, 2, tag, MPI_COMM_WORLD, &status);
    }

    //Doing fft1d forward for data1 and data2 rows
    if(currRank == 0 || currRank == 1){
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data1[i][j];
            }
            fftd_1(vec, N, -1);
            for (j=0;j<N;j++) {
                data1[i][j] = vec[j];
            }
        }

        free(vec);
    }


    if(currRank == 2 || currRank == 3){
        currRank -= 2;
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data2[i][j];
            }
            fftd_1(vec, N, -1);
            for (j=0;j<N;j++) {
                data2[i][j] = vec[j];
            }
        }

        free(vec);
        currRank += 2;
    }

    //Receving the rows from data 1 and data2
    if(currRank == 0){
        for(j = offset; j < N; j++)
            MPI_Recv(data1[j], N, mystruct, 1, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 1){
        for(j = offset; j < N; j++)
            MPI_Send(&data1[j][0], N, mystruct, 0, tag, MPI_COMM_WORLD);
    }

    if(currRank == 2){
        for(j = offset; j < N; j++)
            MPI_Recv(data2[j], N, mystruct, 3, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 3){
        for(j = offset; j < N; j++)
            MPI_Send(&data2[j][0], N, mystruct, 2, tag, MPI_COMM_WORLD);
    }


    //Starting and send columns of data1, data2

    if(currRank == 0){
        transpose(data1, data3);

        for(j = offset; j < N; j++)
            MPI_Send(&data3[j][0], N, mystruct, 1, tag, MPI_COMM_WORLD);
    }

    if(currRank == 1){

        for(j = offset; j < N; j++)
            MPI_Recv(data3[j], N, mystruct, 0, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 2){
        transpose(data2, data4);

        for(j = offset; j < N; j++)
            MPI_Send(&data4[j][0], N, mystruct, 3, tag, MPI_COMM_WORLD);
    }

    if(currRank == 3){

        for(j = offset; j < N; j++)
            MPI_Recv(data4[j], N, mystruct, 2, tag, MPI_COMM_WORLD, &status);
    }

    //Doing fft1d forward for data1 and data2 columns

    if(currRank == 0 || currRank == 1){
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data3[i][j];
            }
            fftd_1(vec, N, -1);
            for (j=0;j<N;j++) {
                data3[i][j] = vec[j];
            }
        }

        free(vec);
    }


    if(currRank == 2 || currRank == 3){
        currRank -= 2;
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data4[i][j];
            }
            fftd_1(vec, N, -1);
            for (j=0;j<N;j++) {
                data4[i][j] = vec[j];
            }
        }

        free(vec);
        currRank += 2;
    }

    //Receving columns of data1, data2

    if(currRank == 0){
        for(j = offset; j < N; j++)
            MPI_Recv(data3[j], N, mystruct, 1, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 1){
        for(j = offset; j < N; j++)
            MPI_Send(&data3[j][0], N, mystruct, 0, tag, MPI_COMM_WORLD);
    }

    if(currRank == 2){
        for(j = offset; j < N; j++)
            MPI_Recv(data4[j], N, mystruct, 3, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 3){
        for(j = offset; j < N; j++)
            MPI_Send(&data4[j][0], N, mystruct, 2, tag, MPI_COMM_WORLD);
    }

    //Send the data to task 3

    if(currRank == 0){
        transpose(data3, data1);

        for(j = 0; j < N; j++)
            MPI_Send(&data1[j][0], N, mystruct, 4, tag, MPI_COMM_WORLD);
    }

    if(currRank == 2){
        transpose(data4, data2);

        for(j = 0; j < N; j++)
            MPI_Send(&data2[j][0], N, mystruct, 4, tag, MPI_COMM_WORLD);
    }


    if(currRank == 4){
        for(j = 0; j < N; j++)
            MPI_Recv(data1[j], N, mystruct, 0, tag, MPI_COMM_WORLD, &status);

        for(j = 0; j < N; j++)
            MPI_Recv(data2[j], N, mystruct, 2, tag, MPI_COMM_WORLD, &status);

        functio_1(data1, data2, data3);
    }

    //Starting and send rows of data3

    if(currRank == 4){

        for(j = offset; j < N; j++)
            MPI_Send(&data3[j][0], N, mystruct, 5, tag, MPI_COMM_WORLD);
    }

    if(currRank == 5){

        for(j = offset; j < N; j++)
            MPI_Recv(data3[j], N, mystruct, 4, tag, MPI_COMM_WORLD, &status);
    }

    //Doing fft1d forward for data1 and data2 rows

    if(currRank == 4 || currRank == 5){
        currRank -= 4;
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data3[i][j];
            }
            fftd_1(vec, N, 1);
            for (j=0;j<N;j++) {
                data3[i][j] = vec[j];
            }
        }

        free(vec);
        currRank += 4;
    }


    //Receving rows of data3

    if(currRank == 4){
        for(j = offset; j < N; j++)
            MPI_Recv(data3[j], N, mystruct, 5, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 5){
        for(j = offset; j < N; j++)
            MPI_Send(&data3[j][0], N, mystruct, 4, tag, MPI_COMM_WORLD);
    }


    //Starting and send columns of data4

    if(currRank == 4){
        transpose(data3, data4);

        for(j = 0; j < N; j++)
            MPI_Send(&data4[j][0], N, mystruct, 6, tag, MPI_COMM_WORLD);
    }

    if(currRank == 6){
        for(j = 0; j < N; j++)
            MPI_Recv(data4[j], N, mystruct, 4, tag, MPI_COMM_WORLD, &status);

        for(j = offset; j < N; j++)
            MPI_Send(&data4[j][0], N, mystruct, 7, tag, MPI_COMM_WORLD);
    }

    if(currRank == 7){
        for(j = offset; j < N; j++)
            MPI_Recv(data4[j], N, mystruct, 6, tag, MPI_COMM_WORLD, &status);
    }

    //Doing fft1d forward for data1 and data2 columns

    if(currRank == 6|| currRank == 7){
        currRank -= 6;
        vec = (complex *)malloc(N * sizeof(complex));

        for (i=currRank*offset;i<(N/2+(currRank*offset));i++) {
            for (j=0;j<N;j++) {
                vec[j] = data4[i][j];
            }
            fftd_1(vec, N, 1);
            for (j=0;j<N;j++) {
                data4[i][j] = vec[j];
            }
        }

        free(vec);
        currRank += 6;
    }

    //Receving columns of data1, data2

    if(currRank == 6){
        for(j = offset; j < N; j++)
            MPI_Recv(data4[j], N, mystruct, 7, tag, MPI_COMM_WORLD, &status);
    }

    if(currRank == 7){
        for(j = offset; j < N; j++)
            MPI_Send(&data4[j][0], N, mystruct, 6, tag, MPI_COMM_WORLD);
    }

    if(currRank == 6){
        transpose(data4,data3);
        /* Stop Clock */
        stopTime = MPI_Wtime();

        printf("\nElapsed time = %lf s.\n",(stopTime - startTime));
        printf("--------------------------------------------\n");
    }

    MPI_Finalize();

    if(currRank == 6){
        printfile(fileName3, data3);
    }

    free(data1);
    free(data2);
    free(data3);
    free(data4);

    return 0;
}
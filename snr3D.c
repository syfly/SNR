/*
 * Signal differences measurements.
 *
 * Compile with:
 *
 *   gcc mse.c -o mse -lfftw3 -lm
 *
 * gse. 2011.
 */

// http://mct.ual.es/wiki/index.php/Snr

// 3 Dimensiones

// La medida del SNR en el dominio de Fourier debe ser una medida supeditada al valor SNR seg�n PSNR, ya que seg�n la FFT
// la medida es invariante frente a la fases espaciales (locales = dentro de una imagen) y temporales (entre im�genes), es 
// decir, da igual en qu� lugar del frame y en qu� frame est� "la cosa" siempre y cuando est�.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include <getopt.h>

#define BLOCK_SIZE 152064	// 352 * 288 * 1.5		// 352*288 + ((352/2)*(288/2))*2   Por componentes, no por n� de p�xeles.
#define DIMENSION_X 352		// Tama�o l�gico, que va para la FFTW de elementos del eje X.
#define DIMENSION_Y 288		// eje Y.
#define DIMENSION_Z 16		// eje Z.
#define PEAK 255

//##############################	##############################
//#						      FUNCIONES							 #
//##############################	##############################

template <class TYPE>

void compute_measures (	char **argv,
						int block_size,
						int dim_x,
						int dim_y,
						int dim_z,
						int FFT,
						FILE *file_A,
						FILE *file_B,
						int peak) {


	int counter = 0;
	double mse, rmse, snr, snr_db, psnr, psnr_db;
	TYPE *buf_A = (TYPE *)malloc(sizeof(TYPE) * block_size * DIMENSION_Z);
	TYPE *buf_B = (TYPE *)malloc(sizeof(TYPE) * block_size * DIMENSION_Z);
	int block;


	// FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT  FFT
	if(FFT) {

		/*--FFT: Specifies the use of the Fourier domain to carry out the comparison. 
		This domain is invariant to a shift of the average amplitude of the signal and 
		a constant shift between the samples, which is much more likely to the way the 
		human beings perceive signals.*/

		// reserva espacio de memoria para las 2 se�ales.
		double *signal_A = (double *)fftw_malloc(sizeof(double) * block_size * DIMENSION_Z);
		double *signal_B = (double *)fftw_malloc(sizeof(double) * block_size * DIMENSION_Z);

		// reserva espacio de memoria para las 2 se�ales en 3 estructuras de datos para FFT
		fftw_complex *fourier_A = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * block_size * DIMENSION_Z); // dim_x * (dim_y/2+1)
		fftw_complex *fourier_B = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * block_size * DIMENSION_Z);

		fftw_plan f_A = fftw_plan_dft_r2c_3d(dim_x, dim_y, dim_z, signal_A, fourier_A, 0);	// 0  �  FFTW_ESTIMATE  �  FFTW_MEASURE  http://www.fftw.org/doc/Complex-One_002dDimensional-DFTs.html
		fftw_plan f_B = fftw_plan_dft_r2c_3d(dim_x, dim_y, dim_z, signal_B, fourier_B, 0);

		// El espectro es invariante de la fase. Se comparan los espectros.
		double *spectrum_A = (double *)malloc(sizeof(double) * block_size * DIMENSION_Z);
		double *spectrum_B = (double *)malloc(sizeof(double) * block_size * DIMENSION_Z);


		// snr con FFT
		double energy_A=0, energy_B=0, energy_error=0; // unsigned
		printf("%s: performing the FFT!\n",argv[0]);
		
		int i;
		for(block = 0;; block * DIMENSION_Z) { // Contabiliza la cantidad de energia de cada se�al y su diferencia para cada porci�n de las se�ales, de tama�o block (cuadro del v�deo)

			double local_energy_error = energy_error, local_energy_A = energy_A;
			int local_counter = counter;
			int r_A = fread(buf_A, sizeof(TYPE), block_size * DIMENSION_Z, file_A);
			int r_B = fread(buf_B, sizeof(TYPE), block_size * DIMENSION_Z, file_B);
			// Establece la condici�n del bucle: hasta que una de las 2 se�ales se termine.
			int min = r_B; 
			if(r_A < r_B) min = r_A;

			// Populates and execution.
				// A
				if(min==0) break; {
					for(i=0; i<min; i++) {
						signal_A[i] = buf_A[i];
					}
				}
				fftw_execute(f_A);
				for(i=0; i<min/2; i++) {
					spectrum_A[i] = sqrt(fourier_A[i][0]*fourier_A[i][0] + fourier_A[i][1]*fourier_A[i][1]);
				}

				// B
				if(min==0) break; {
					for(i=0; i<min; i++) {
						signal_B[i] = buf_B[i];
					}
				}
				fftw_execute(f_B);
				for(i=0; i<min/2; i++) {
					spectrum_B[i] = sqrt(fourier_B[i][0]*fourier_B[i][0] + fourier_B[i][1]*fourier_B[i][1]);
				}

			// Contabiliza la cantidad de energia de cada se�al y su diferencia.
			double a,b,ab;
			for(i=0; i<min/2; i++) {
				a = spectrum_A[i];		// porci�n de la se�al A (seguramente p�xel)
				b = spectrum_B[i];		// porci�n de la se�al B (seguramente p�xel)
				energy_A += a*a;		// acumulaci�n de la energ�a de la se�al A
				energy_B += b*b;		// acumulaci�n de la energ�a de la se�al B
				ab = a - b;				// diferencia = error entre ambas se�ales
				energy_error += ab*ab;	// acumulaci�n del error entre ambas se�ales
				counter++;				// resulta con el tama�o de la porci�n de se�al que se est� tratando (seguramente una imagen)
			}

			// -------- local_energy_error = error para cada block (para cada imagen)
			if(energy_error) { // Ambas se�ales son distintas (a nivel de imagen)

				local_energy_error = energy_error - local_energy_error;	// por la forma en que se va contabilizando "energy_error" para tener la "local_energy_error" se resta todo el error que hemos ido contando que est� en "energy_error" MENOS el error de la im�gen anterior, que est� en "local_energy_error".
				local_counter = counter - local_counter;				// "
				local_energy_A = energy_A - local_energy_A;				// "

				mse=local_energy_error/(double)(local_counter);
				psnr=(double)(peak)*(double)(peak)/mse;
				psnr_db=10.0*log10(psnr);
				rmse=sqrt(mse);
				snr=local_energy_A/local_energy_error;
				snr_db=10.0*log10(snr);

			} else { // Ambas se�ales son iguales (a nivel de imagen)

				snr_db=1.0;
				psnr_db=1.0;
				//printf("SNR infinito !!!\n");
				//return 1;
			}
				fprintf(stderr,"%3d\t%f\n", block, psnr_db);
		} // for(block = 0;; block++)

		
		// -------- Calcula resultados
		if(energy_error) {

			mse=(double)(energy_error)/(double)(counter);
			psnr=(double)(peak)*(double)(peak)/mse;
			psnr_db=10.0*log10(psnr);
			rmse=sqrt(mse);
			snr=(double)(energy_A)/(double)(energy_error);
			snr_db=10.0*log10(snr);

		} else {

			mse=0.0;
			psnr=1.0;
			psnr_db=1.0;
			rmse=0.0;
			snr=1.0;
			snr_db=1.0;
			//printf("SNR infinito !!!\n");
			//return 1;
		}
      
		// -------- RESULTADOS
		printf("Energy_A\t=\t%f\n",energy_A);
		printf("Energy_B\t=\t%f\n",energy_B);
		printf("Energy_error\t=\t%f\n",energy_error);
		printf("Number of samples\t=\t%d\n",counter);
		printf("MSE\t=\t%f\n",mse);
		printf("RMSE\t=\t%f\n",rmse);
		printf("SNR\t=\t%f\n",snr);
		printf("SNR[dB]\t=\t%f\n",snr_db);
		printf("PSNR\t=\t%f\n",psnr);
		printf("PSNR[dB]\t=\t%f\n",psnr_db);
    
		// -------- Free memory
		fftw_destroy_plan(f_A);
		fftw_destroy_plan(f_B);
		fftw_free(fourier_A);
		fftw_free(fourier_B);




	// !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT  !FFT
	} else { // P�g. 231 SNR

		// snr sin FFT
		long long energy_A=0, energy_B=0, energy_error=0; // unsigned

		for(block = 0;; block++) { // Contabiliza la cantidad de energia de cada se�al y su diferencia para cada porci�n de las se�ales, de tama�o block (cuadro del video)

			long long local_energy_error = energy_error, local_energy_A = energy_A;
			int local_counter = counter;
			int r_A = fread(buf_A,sizeof(TYPE),block_size,file_A);	// Lee la primera imagen de la se�al A (original)
			int r_B = fread(buf_B,sizeof(TYPE),block_size,file_B);  // Lee la primera imagen de la se�al B (reconstruido)

			// Establece la condici�n del bucle: hasta que una de las 2 se�ales se termine.
			int min = r_B;
			if(r_A < r_B) min = r_A;

			// Contabiliza la cantidad de energia de cada se�al y su diferencia.
			if(min==0) break; {
				int a,b,ab;
				for(int i=0; i<min; i++) {
					a = buf_A[i]; 			// porci�n de la se�al A (seguramente p�xel)
					b = buf_B[i]; 			// porci�n de la se�al B (seguramente p�xel)
					energy_A += a*a; 		// acumulaci�n de la energ�a de la se�al A
					energy_B += b*b; 		// acumulaci�n de la energ�a de la se�al B
					ab = a - b;				// diferencia = error entre ambas se�ales
					energy_error += ab*ab; 	// acumulaci�n del error entre ambas se�ales
					counter++;				// resulta con el tama�o de la porci�n de se�al que se est� tratando (seguramente una imagen)
				}
			}

			// -------- local_energy_error = error para cada block (para cada imagen)
			if(energy_error) { // Ambas se�ales son distintas (a nivel de imagen)

				local_energy_error = energy_error - local_energy_error;		// por la forma en que se va contabilizando "energy_error" para tener la "local_energy_error" se resta todo el error que hemos ido contando que est� en "energy_error" MENOS el error de la im�gen anterior, que est� en "local_energy_error".
				local_counter = counter - local_counter;					// "
				local_energy_A = energy_A - local_energy_A;					// "

				mse=(double)(local_energy_error)/(double)(local_counter);
				psnr=(double)(peak)*(double)(peak)/mse;
				psnr_db=10.0*log10(psnr);
				rmse=sqrt(mse);
				snr=(double)(local_energy_A)/(double)(local_energy_error);
				snr_db=10.0*log10(snr);

			} else { // Ambas se�ales son iguales (a nivel de imagen)

				snr_db=1.0;
				psnr_db=1.0;
				//printf("SNR infinito !!!\n");
				//return 1;
			}
		fprintf(stderr,"%3d\t%f\n", block, psnr_db);
		} // for(block = 0;; block++)


		// -------- Calcula resultados
		if(energy_error) { // Ambas se�ales son distintas (a nivel de video)

			mse=(double)(energy_error)/(double)(counter);
			psnr=(double)(peak)*(double)(peak)/mse;
			psnr_db=10.0*log10(psnr);
			rmse=sqrt(mse);
			snr=(double)(energy_A)/(double)(energy_error);
			snr_db=10.0*log10(snr);

		} else { // Ambas se�ales son iguales (a nivel de video)
			mse=0.0;
			psnr=1.0;
			psnr_db=1.0;
			rmse=0.0;
			snr=1.0;
			snr_db=1.0;
			//printf("SNR infinito !!!\n");
			//return 1;
    	}

		// -------- RESULTADOS
		printf("Energy_A\t=\t%Ld\n",energy_A);
		printf("Energy_B\t=\t%Ld\n",energy_B);
		printf("Energy_error\t=\t%Ld\n",energy_error);
		printf("Number of samples\t=\t%d\n",counter);
		printf("MSE\t=\t%f\n",mse);
		printf("RMSE\t=\t%f\n",rmse);
		printf("SNR\t=\t%f\n",snr);
		printf("SNR[dB]\t=\t%f\n",snr_db);
		printf("PSNR\t=\t%f\n",psnr);
		printf("PSNR[dB]\t=\t%f\n",psnr_db);
  }
} //void compute_measures


/* HELP template */

void help(void) {
  printf("snr3D \n");
  printf(" --type={[uchar]|char|ushort|short|uint|int}\n");
  printf(" --peak=[255]\n");
  printf(" --file_A=First file to compare.\n");
  printf(" --file_B=The second file.\n");
  printf(" --block_size=<block size in bytes=[%d]>\n",BLOCK_SIZE);
  printf(" --dim_X=<x-axis dimension=[%d]>\n",DIMENSION_X);
  printf(" --dim_Y=<y-axis dimension=[%d]>\n",DIMENSION_Y);
  printf(" --dim_Z=<z-axis dimension=[%d]>\n",DIMENSION_Z);
  printf(" --FFT={yes|[no]}\n");
  printf("\n");
  printf("Compute:\n");
  printf("               N-1\n");
  printf("   Energy(A) = Sum A[i]^2\n");
  printf("               i=0\n");
  printf("\n");
  printf("               N-1\n");
  printf("   Energy(B) = Sum B[i]^2\n");
  printf("               i=0\n");
  printf("\n");
  printf("         Energy(A)\n");
  printf("   SNR = ---------              (Signal-to-Noise Ratio)\n");
  printf("         Energy(B)\n");
  printf("\n");
  printf("          1  N-1\n");
  printf("   MSE = --- Sum (A[i]-B[i])^2  (Mean Square Error)\n");
  printf("          N  i=0\n");
  printf("\n");
  printf("   RMSE = sqrt(MSE)             (Root Mean Square Error)\n");
  printf("\n");
  printf("   SNR[dB] = 10 log SNR\n");
  printf("\n");
  printf("           peak^2 \n");
  printf("   PSNR = --------              (Peak SNR)\n");
  printf("             MSE \n");
  printf("\n");
  printf("   PSNR[dB] = 10 log PSNR\n");
  printf("\n");
  printf("between file_A and file_B\n");
}




//##############################	##############################
//#						      	MAIN							 #
//##############################	##############################

int main (int argc, char *argv[]) {

	char *type = (char *)"uchar";
	FILE *file_A, *file_B;
	int peak = PEAK;
	int block_size = BLOCK_SIZE;
	int dim_X = DIMENSION_X;
	int dim_Y = DIMENSION_Y;
	int dim_Z = DIMENSION_Z;
	int FFT = 0;
	printf("%d\n",argc);
	if(argc==1) {
		help();
		exit(1);
	}

	while(1) {
		static struct option long_options[] = {
		{"block_size",	required_argument, 0, 'b'},
		{"dim_X",		required_argument, 0, 'x'},
		{"dim_Y",		required_argument, 0, 'y'},
		{"dim_Z",		required_argument, 0, 'z'},
		{"file_A",		required_argument, 0, 'A'},
		{"file_B",		required_argument, 0, 'B'},
		{"peak",		required_argument, 0, 'p'},
		{"type",		required_argument, 0, 't'},
		{"FFT",			no_argument,       0, 'F'},
		{"help",		no_argument,       0, '?'},
		{0, 0, 0, 0}
    };

	int option_index = 0;
    
	int c = getopt_long(argc, argv,
		    "t:p:A:B:b:x:y:z:F?",
		    long_options, &option_index);
    
    if(c==-1) {
      /* Ya no hay m�s opciones. */
      break;
    }
    
    switch (c) {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
	break;
      printf("option %s", long_options[option_index].name);
      if (optarg)
		printf(" with arg %s", optarg);
	    printf ("\n");
      break;
      
    case 't':
      type = optarg;
      printf("%s: type = %s\n", argv[0], type);
      break;
      
    case 'p':
      peak = atoi(optarg);
      printf("%s: peak = %d\n", argv[0], peak);
      break;
      
    case 'A':
      file_A=fopen(optarg,"rb");
      if(!file_A) {
		printf("%s: unable to open input file \"%s\"\n",argv[0],optarg);
		return 1;
      }
      break;
      
    case 'B':
      file_B=fopen(optarg,"rb");
      if(!file_B) {
		printf("%s: unable to open input file \"%s\"\n",argv[0],optarg);
		return 2;
      }
      break;
      
    case 'b':
      block_size = atoi(optarg);
      printf("%s: block_size = %d\n", argv[0], block_size);
      break;

    case 'F':
      FFT = 1;
      printf("%s: Fast Fourier Transform = yes\n", argv[0]);
      break;

    case 'x':
      dim_X = atoi(optarg);
      printf("%s: X dimension = %d\n", argv[0], dim_X);
      break;

    case 'y':
      dim_Y = atoi(optarg);
      printf("%s: Y dimension = %d\n", argv[0], dim_Y);
      break;

    case 'z':
      dim_Z = atoi(optarg);
      printf("%s: Z dimension = %d\n", argv[0], dim_Z);
      break;

    case '?':
      help();
      exit(1);

    default:
      abort ();
    }
  }

  if (!strcmp(type,"uchar")) {
    printf("%s: data type: uchar\n",argv[0]);
    compute_measures<unsigned char>(argv, block_size, dim_X, dim_Y, dim_Z, FFT, file_A, file_B, peak);
  }

  if (!strcmp(type,"char")) {
    printf("%s: data type: char\n",argv[0]);
    compute_measures<char>(argv, block_size, dim_X, dim_Y, dim_Z, FFT, file_A, file_B, peak);
  }
 
  if (!strcmp(type,"ushort")) {
    printf("%s: data type: ushort\n",argv[0]);
    compute_measures<unsigned short>(argv, block_size, dim_X, dim_Y, dim_Z, FFT, file_A, file_B, peak);
  }
 
  if (!strcmp(type,"short")) {
    printf("%s: data type: short\n",argv[0]);
    compute_measures<short>(argv, block_size, dim_X, dim_Y, dim_Z, FFT, file_A, file_B, peak);
  }

}

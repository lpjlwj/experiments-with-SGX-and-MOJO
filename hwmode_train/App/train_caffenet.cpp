// == mojo ====================================================================
//
//    Copyright (c) gnawice@gnawice.com. All rights reserved.
//	  See LICENSE in root folder
//
//    Permission is hereby granted, free of charge, to any person obtaining a
//    copy of this software and associated documentation files(the "Software"),
//    to deal in the Software without restriction, including without 
//    limitation the rights to use, copy, modify, merge, publish, distribute,
//    sublicense, and/or sell copies of the Software, and to permit persons to
//    whom the Software is furnished to do so, subject to the following 
//    conditions :
//
//    The above copyright notice and this permission notice shall be included
//    in all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
//    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
//    OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
//    THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// ============================================================================
//    train_caffenet.cpp:  train caffenet classifier
//
//    Instructions: 
//	  Add the "mojo" folder in your include path.
//    Download MNIST data and unzip locally on your machine:
//		(http://yann.lecun.com/exdb/mnist/index.html)
//    Set the data_path variable in the code to point to your data location.
// ==================================================================== mojo ==


#include "Enclave_u.h"
#include "sgx_urts.h"

#include <iostream> // cout
#include <vector>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

#include <mojo.h>
#include <util.h>
#include <unistd.h>

//*
#include "parser/imagenet_parser.h"



std::string solver = "adam";
std::string data_path="../data/";
using namespace imagenet;

const int mini_batch_size = 24; // also defined in Enclave.cpp


/* Global EID shared by multiple threads */
sgx_enclave_id_t eid = 0;

// OCall implementations
void ocall_print(const char* str) {
	printf("%s\n", str);
}

void mojo_sleep(unsigned milliseconds) 
{ 
	mojo::usleep(milliseconds * 1000); // ?
} 

FILE *f = NULL;
void open_file(const char* str)
{
	f = fopen(str, "r");
	if(!f)
	{
		printf("open image file error.\n");
		exit(0);
	}
}

void read_file(char *dest, int sz)
{
	fread(dest, sz, 1, f);
}

void close_file()
{
	if(f) fclose(f);
}

// deep network file operations follow
FILE *fnetwork = NULL;
FILE *output_network_file = NULL;

int open_networkfile(const char* str)
{
	fnetwork = fopen(str, "rb");
	if(!fnetwork)
	{
		printf("open deep network file error.\n");
		return 1;
	}

	return 0;
}

int open_outputnetworkfile(const char* str)
{
	output_network_file = fopen(str, "wb");
	if(!output_network_file)
	{
		printf("open deep network file error.\n");
		return 1;
	}

	return 0;
}

// OCall implementations, for open_outputnetworkfile
void ocall_fprint_networkfile(const char* str) {
	//    printf("%s", str);
	fprintf(output_network_file, "%s", str);
}

void ocall_write(char *src, int sz)
{
	//    printf("size: %d, src[%d-1]: %d\n", sz, sz, src[sz-1]);
	fwrite(src, 1, sz, output_network_file);
}
/////////////

// OCall implementations
char ocall_fread_networkfile() {
	char ret = fgetc(fnetwork);
}

int ocall_getint()
{
	int ret;
	fscanf(fnetwork, "%d", &ret);

	return ret;
}

float ocall_getfloat()
{
	float ret;
	fscanf(fnetwork, "%f", &ret);

	return ret;
}

void ocall_read(char *src, int sz)
{
	fread(src, 1, sz, fnetwork);
}

void end_this_line()
{
	char s[256];

	fgets(s, 256, fnetwork);
}

void close_networkfile()
{
	fflush(fnetwork);
	if(fnetwork) fclose(fnetwork);
}

void close_outputnetworkfile()
{
	//    fflush(output_network_file);
	if(output_network_file) fclose(output_network_file);
}


// performs validation testing
float test(const std::vector<std::vector<float>> &test_images, const std::vector<int> &test_labels)
{
	// use progress object for simple timing and status updating
	mojo::progress progress((int)test_images.size(), "  testing:\t\t");

	int out_size;
	cnn_outsize(eid, &out_size); // we know this to be 10 for MNIST and CIFAR

	int correct_predictions = 0;
	const int record_cnt = (int)test_images.size();

	//	#pragma omp parallel for reduction(+:correct_predictions) schedule(dynamic)
	for (int k = 0; k<record_cnt; k++)
	{
		int prediction = 0;

		//printf("test image size: %d\n", test_images[k].size());
		classification(eid, &prediction, (float *)test_images[k].data(), test_images[k].size()); // input data
		//std::cout<<"prediction: "<<prediction<<std::endl;
		if (prediction == test_labels[k]) correct_predictions += 1;
		if (k % 100 == 0) progress.draw_progress(k);
	}

	float accuracy = (float)correct_predictions / record_cnt*100.f;
	return accuracy;
}


int main()
{
	sgx_status_t        ret = SGX_SUCCESS;
	sgx_launch_token_t  token = { 0 };
	int updated = 0;

	ret = sgx_create_enclave("enclave.signed.so", SGX_DEBUG_FLAG, &token, &updated, &eid, NULL);
	if (ret != SGX_SUCCESS)
		return -1;

	printf("Enclave loaded.\n");

	// ==== parse data
	// array to hold image data (note that mojo does not require use of std::vector)
	std::vector<std::vector<float>> test_images;
	std::vector<int> test_labels;
	std::vector<std::vector<float>> train_images;
	std::vector<int> train_labels;

	// calls MNIST::parse_test_data  or  IMAGENET::parse_test_data depending on 'using'
	if (!parse_test_data(data_path, test_images, test_labels)) { std::cerr << "error: could not parse data.\n"; return 1; }
	if (!parse_train_data(data_path, train_images, train_labels)) { std::cerr << "error: could not parse data.\n"; return 1; }

	// ==== setup the network  - when you train you must specify an optimizer ("sgd", "rmsprop", "adagrad", "adam")

	new_network(eid, "../models/caffenet.txt");
	std::cout << "new_network created.\n";


	//	std::cout << "==  Network Configuration  ====================================================" << std::endl;
	//	std::cout << cnn.get_configuration() << std::endl;

	// add headers for table of values we want to log out
	mojo::html_log log;
	log.set_table_header("epoch\ttest accuracy(%)\testimated accuracy(%)\tepoch time(s)\ttotal time(s)\tlearn rate\tmodel");
	//	log.set_note(cnn.get_configuration());

	// augment data random shifts only
	//	cnn.set_random_augmentation(1,1,0,0,mojo::edge);

	// setup timer/progress for overall training
	mojo::progress overall_progress(-1, "  overall:\t\t");
	const int train_samples = (int)train_images.size();
	float old_accuracy = 0; 
	while (1)
	{
		int _epoch = 0;
		get_epoch(eid, &_epoch);
		overall_progress.draw_header(data_name() + "  Epoch  " + std::to_string((long long)_epoch + 1), true);
		// setup timer / progress for this one epoch
		mojo::progress progress(train_samples, "  training:\t\t");
		// set loss function

		epoch(eid, "cross_entropy");
		//		cnn.start_epoch("cross_entropy");

		// manually loop through data. batches are handled internally. if data is to be shuffled, the must be performed externally
		//		#pragma omp parallel for schedule(dynamic)  // schedule dynamic to help make progress bar work correctly
		for (int k = 0; k<train_samples; k++)
		{
			//cnn.train_class(train_images[k].data(), train_labels[k]);
			train(eid, train_images[k].data(), train_images[k].size(), train_labels[k]);
			if (k % 100 == 0) progress.draw_progress(k); 
		}

		end_epoch(eid);
		//cnn.end_epoch();
		float dt = progress.elapsed_seconds();
		float estimated_accuracy = 0.0;
		get_estimated_accuracy(eid, &estimated_accuracy);

		std::cout << "  mini batch:\t\t" << mini_batch_size << "                               " << std::endl;
		std::cout << "  training time:\t" << dt << " seconds."<< std::endl;
		//		std::cout << "  model updates:\t" << cnn.train_updates << " (" << (int)(100.f*(1. - (float)cnn.train_skipped / cnn.train_samples)) << "% of records)" << std::endl;
		std::cout << "  estimated accuracy:\t" << estimated_accuracy << "%" << std::endl;


		/* if you want to run in-sample testing on the training set, include this code
		// == run training set
		progress.reset((int)train_images.size(), "  testing in-sample:\t");
		float train_accuracy=test(cnn, train_images, train_labels);
		std::cout << "  train accuracy:\t"<<train_accuracy<<"% ("<< 100.f - train_accuracy<<"% error)      "<<std::endl;
		 */

		// ==== run testing set
		progress.reset((int)test_images.size(), "  testing out-of-sample:\t");
		float accuracy = test(test_images, test_labels);
		std::cout << "  test accuracy:\t" << accuracy << "% (" << 100.f - accuracy << "% error)      " << std::endl;

		// if accuracy is improving, reset the training logic that may be thinking about quitting
		if (accuracy > old_accuracy)
		{
			reset_smart_training(eid);
			old_accuracy = accuracy;
		}

		cout<<">>>>>>>>>>>>>>>>>>>>"<<endl;

		// save model
		/*		std::string model_file = "../models/snapshots/caffenet_tmp_" + std::to_string((long long)_epoch) + ".txt";
				write_model_file(eid, (char *)model_file.c_str());
		//		cnn.write(model_file,true);
		std::cout << "  saved model:\t\t" << model_file << std::endl << std::endl;

		// write log file
		std::string log_out;
		log_out += float2str(dt) + "\t";
		log_out += float2str(overall_progress.elapsed_seconds()) + "\t";
		//		log_out += float2str(cnn.get_learning_rate()) + "\t";
		log_out += model_file;
		log.add_table_row(estimated_accuracy, accuracy, log_out);
		// will write this every epoch
		log.write("../models/snapshots/mojo_caffenet_log.htm");
		 */
		// can't seem to improve
		int elvisleft;
		elvis_left_the_building(eid, &elvisleft);
		if (elvisleft)
		{
			std::cout << "Elvis just left the building. No further improvement in training found.\nStopping.." << std::endl;
			break;
		}

	};
	std::cout << std::endl;

	sgx_destroy_enclave(eid);
	return 0;
}

#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <time.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <iostream> // cout
#include <sstream>
#include <fstream>
#include <iomanip> //setw
#include <random>


using namespace cv;
using namespace std;

namespace imagenet
{
std::string data_name() {return std::string("IMAGENET");}

bool parse_test_data(std::string &data_path, std::vector<std::vector<float>> &test_images, std::vector<int> &test_labels, 
	float min_val=-1.f, float max_val=1.f, int x_padding=0, int y_padding=0)
{
   // x_padding = 0;
   // y_padding = 0;
	ifstream label_val(data_path+"val.txt");

    vector<pair<string, int> > lines;
    string line;
    size_t pos;
    int label;
    while (getline(label_val, line)) 
    {
        pos = line.find_last_of(' ');
        label = atoi(line.substr(pos + 1).c_str());
        lines.push_back(make_pair(line.substr(0, pos), label));
    }
  
 //   random_shuffle(lines.begin(), lines.end());
  
    for (int line_id = 0; line_id < lines.size(); ++line_id) 
    {
        string filename = lines[line_id].first;
        int label = lines[line_id].second;
        
        string imagepath = data_path+"/val/"+filename;
        cv::Mat im = cv::imread(imagepath);
	    if(im.empty() || im.cols<1) { std::cout << "Failed to read a valid image. (" << data_path <<")"<<std::endl; return 1;}

	    // resize to 224x224 BGR format
	    cv::resize(im,im,cv::Size(227, 227));
	    // make 3 chan
	    if (im.channels()<3) cv::cvtColor(im,im,CV_GRAY2BGR);

	    // convert packed BGR to planar BGR and subtract IMAGENET mean (while converting to float)
	    cv::Mat bgr[3];
	    cv::split(im, bgr);
	    //float *img = new float [3*224*224]; 
	    int width = 227+2*x_padding;
		int height = 227+2*y_padding;
	    std::vector<float> image(height*width*3);
	    
	    for(int i=0; i<227*227; i++)
	    {
		    image[i+227*227*0]= (float)bgr[2].data[i] - 85.5297f;
		    image[i+227*227*1]= (float)bgr[1].data[i] - 105.394f;
		    image[i+227*227*2]= (float)bgr[0].data[i] - 106.87f;  // mean for imagenet dataset
	    }
        test_images.push_back(image);      
        test_labels.push_back(label);
        
  //      cout<<imagepath<<" "<<label<<endl;
    }
	label_val.close();
	return true;
}

bool parse_train_data(std::string &data_path, std::vector<std::vector<float>> &train_images, std::vector<int> &train_labels, 
	float min_val=-1.f, float max_val=1.f, int x_padding=0, int y_padding=0)
{
 //   x_padding = 0;
 //   y_padding = 0;
	ifstream label_train(data_path+"/train.txt");
	
	vector<pair<string, int> > lines;
    string line;
    size_t pos;
    int label;
    while (getline(label_train, line)) 
    {
        pos = line.find_last_of(' ');
        label = atoi(line.substr(pos + 1).c_str());
        lines.push_back(make_pair(line.substr(0, pos), label));
    }
  
    random_shuffle(lines.begin(), lines.end());
  
    for (int line_id = 0; line_id < lines.size(); ++line_id) 
    {
        string filename = lines[line_id].first;
        int label = lines[line_id].second;
              
        string imagepath = data_path+"/train/"+filename;
  //      cout<<"No. "<<line_id<<": "<<imagepath<<" "<<label<<endl;
        cv::Mat im = cv::imread(imagepath);
	    if(im.empty() || im.cols<1) { std::cout << "Failed to read a valid image. (" << data_path <<")"<<std::endl; return 1;}

	    // resize to 224x224 BGR format
	    cv::resize(im,im,cv::Size(227, 227));
	    // make 3 chan
	    if (im.channels()<3) cv::cvtColor(im,im,CV_GRAY2BGR);

	    // convert packed BGR to planar BGR and subtract VGG mean (while converting to float)
	    cv::Mat bgr[3];
	    cv::split(im, bgr);
	    //float *img = new float [3*224*224]; 
	    int width = 227+2*x_padding;
		int height = 227+2*y_padding;
	    std::vector<float> image(height*width*3);
	    
	    for(int i=0; i<227*227;i++)
	    {
		    image[i+227*227*0]= (float)bgr[2].data[i] - 85.5297f;
		    image[i+227*227*1]= (float)bgr[1].data[i] - 105.394f;
		    image[i+227*227*2]= (float)bgr[0].data[i] - 106.87f;  // mean for imagenet dataset
		    
	//	    cout<<image[i+227*227*0]<<" "<<image[i+227*227*1]<<" "<<image[i+227*227*2]<<" ("<<(unsigned short)bgr[2].data[i]<<")."<<endl;
	    }
       
        train_images.push_back(image);      
        train_labels.push_back(label);
        

  //      cout<<">>> processed file: "<<imagepath<<endl;
    }
    
   // cout<<"finished parsing training dataset"<<endl;
	label_train.close();
	return true;
}
}

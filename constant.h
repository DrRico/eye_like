#ifndef CONSTANT_H
#define CONSTANT_H

using namespace std;
using namespace cv;

char local_ip[0x10];
char dev[0xc];
string client_ip;
string recvdata;
string senddata;
int port = 8080;

// Face init
bool isinited;

// Eye Region
const int left_eye_x = 140;
const int left_eye_y = 60;
const int eye_region_width = 80;
const int eye_region_height = 30;

const int resizex = 50;
const int resizey = 25;
const int camera_flag = 0;

const int eye_radii = 6;

#endif
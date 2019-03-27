#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <vector>
#include <signal.h>
#include <malloc.h>
#include "TCPServer.h"
#include "constant.h"
#include "FindEyeCenter.h"

using namespace cv;
using namespace std;

TCPServer tcp;

//输出使用方法
void usage(char *argv0)
{
    cout << "Usage:" << endl;
    cout << "\t" << argv0 << " device" << endl;
    cout << "\tExample:" << argv0 << " en0" << endl;
    exit(1);
}

//IP地址初始化,获取指定网卡上的IP地址
void ip_init()
{
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        perror("Device init");
        exit(1);
    }

    struct ifreq addr;

    memset(&addr, 0x0, sizeof addr);
    strcpy(addr.ifr_name, dev);
    if (ioctl(sockfd, SIOCGIFADDR, (char *)&addr) == -1)
    {
        perror("Device init");
        exit(1);
    }
    strcpy(local_ip, inet_ntoa(((struct sockaddr_in *)&addr.ifr_addr)->sin_addr));
    close(sockfd);
}

//发送大图
void send_big_img(vector<unsigned char> &mat)
{
    string str(mat.begin(), mat.end());
    str.insert(0, "6");
    tcp.Send(str);
}

//发送眼部图
void send_img(vector<unsigned char> &mat)
{
    string str(mat.begin(), mat.end());
    str.insert(0, "5");
    tcp.Send(str);
}

//发送右指令
void right()
{
    tcp.Send("1");
}

//发送左指令
void left()
{
    tcp.Send("2");
}

//发送确认指令
void ensure()
{
    tcp.Send("3");
}

//发送初始化指令
void init()
{
    tcp.Send("4");
}

//处理ctrl + c,会进入TIME_WAIT状态,所以尽量不要关闭,减少麻烦
void handle_ctrl_c(int sig)
{
    cout << "\nYou Pressed Ctrl+C" << endl;
    tcp.detach();
    sleep(0.1);
    exit(0);
}

// 脸部初始化,对准矩形框
bool face_init()
{
    VideoCapture capture(camera_flag);
    capture.set(CV_CAP_PROP_FRAME_WIDTH, 320); //设置分辨率
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
    Mat frame;
    vector<unsigned char> frame_encode;
    Rect rect_eye(left_eye_x, left_eye_y, eye_region_width, eye_region_height);
    while (1)
    {
        capture >> frame;
        flip(frame, frame, 0);
        rectangle(frame, rect_eye, Scalar(0, 255, 0), 2, 1, 0);
        imencode(".jpg", frame, frame_encode);
        send_big_img(frame_encode);
        recvdata = tcp.Recv();
        if (recvdata == "0")
        {
            tcp.disconnect();
            capture.release();
            return false;
        }
        else if (recvdata == "8")
        {
            capture.release();
            return true;
        }
    }
}

//判断左右loop循环
int loop()
{
    VideoCapture capture(camera_flag);
    capture.set(CV_CAP_PROP_FRAME_WIDTH, 320); //设置分辨率
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, 240);
    Mat frame;
    Mat eyeROI;
    Mat eyeGRAY;
    Rect rect_eye(left_eye_x, left_eye_y, eye_region_width, eye_region_height);
    vector<unsigned char> frame_encode;
    //增加判断左右的变量
    bool eyeon_left = false,eyeon_right = false,eyeon_center = false,eye_stute = false;
    unsigned char eye_counter = 2;
    while (1)
    {
        capture >> frame;
        flip(frame, frame, 0);
        eyeROI = frame(rect_eye);
        resize(eyeROI, eyeROI, Size(50, 25));
        cvtColor(eyeROI, eyeGRAY, CV_BGR2GRAY);
        Point loc = findEyeCenter(eyeGRAY, eye_radii);
        circle(eyeROI, loc, 3, Scalar(0, 255, 0), -1, 8);

        if((loc.x > 30 && loc.y > 18) && eye_stute)
        {
            eye_stute = false;
            if(!(--eye_counter))
            {
                eye_counter = 2;
                ensure();
                std::cout << "comfirm ! ! !"<< endl;

            }
            continue;
        }
        else if((loc.x < 25 && loc.x > 18) && (loc.y > 10 && loc.y < 16))
        {
            eye_stute = true;
            eyeon_left = false;
            eyeon_right = false;
            std::cout << "eye on center~~~~~"<< endl;
            continue;
        }
        if(loc.x > 25 && (!eyeon_left) && (!eyeon_right))
        (
            std::cout << "---------left>>>>>>>>>"<< endl;
            eyeon_left = true;
            eye_counter = 2;
            left();
            continue;
        )
        if(loc.x < 18 && (!eyeon_left) && (!eyeon_right))
        {
            std::cout << "<<<<<<<<<right---------"<< endl;
            eyeon_right = true;
            eye_counter = 2;
            right();
            continue;
        }
        /*imshow("eye circle", eyeROI);
        int Key = waitKey(1);
        if (Key == 13)
        {
            destroyAllWindows();
            capture.release();
            break;
        }*/
        imencode(".jpg", eyeROI, frame_encode);
        send_img(frame_encode);
        recvdata = tcp.Recv();
        if (recvdata == "0")
        {
            tcp.disconnect();
            capture.release();
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &handle_ctrl_c);

    if (argc != 2)
    {
        usage(argv[0]);
    }
    strcpy(dev, argv[1]);
    ip_init();

    tcp.setup(local_ip, port);

    int ret;
    while (1)
    {
        client_ip = tcp.receive();
        cout << "Client:" << client_ip << endl;
        isinited = face_init();
        if (isinited)
        {
            ret = loop();
            cout << ret << endl;
        }
    }
    tcp.detach();
    return 0;
}

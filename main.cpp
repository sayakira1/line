#include "opencv2/opencv.hpp"
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "dxl.hpp"
using namespace std;
using namespace cv;
bool ctrl_c_pressed;
void ctrlc(int)
{
    ctrl_c_pressed = true;
}
int main() {
    string src = "nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)640, \
    height=(int)360, format=(string)NV12 ! \
    nvvidconv flip-method=0 ! video/x-raw, width=(int)640, height=(int)360, \
    format=(string)BGRx ! videoconvert ! \
    video/x-raw, format=(string)BGR !appsink";
    string dst1= "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc \
    insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.161 \
    port=8001 sync=false";
    string dst2= "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc \
    insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.161 \
    port=8002 sync=false";
    // 영상 객체 생성
    VideoCapture cap(src,CAP_GSTREAMER);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video file." << std::endl;
        return -1;
    }
    
    VideoWriter writer1(dst1, 0, (double)30, cv::Size(640, 360), true);
    if (!writer1.isOpened()) { cerr << "Writer open failed!" << endl; return -1;}
    signal(SIGINT, ctrlc);
    VideoWriter writer2(dst2, 0, (double)30, cv::Size(640, 90), true);
    if (!writer2.isOpened()) { cerr << "Writer open failed!" << endl; return -1;}
    signal(SIGINT, ctrlc);
    
    Dxl mx;
    struct timeval start,end1;
    double diff1;
    signal(SIGINT, ctrlc);
    if (!mx.open()) { cout << "dynamixel open error" << endl; return -1; } //장치열기

    // 영상 프레임 및 FPS 확인
    Size frameSize = Size((int)cap.get(cv::CAP_PROP_FRAME_WIDTH), (int)cap.get(CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(CAP_PROP_FPS);

    int roiHeight = frameSize.height / 4;
    Rect roiRect(0, frameSize.height - roiHeight, frameSize.width, roiHeight);

    // 영상 저장 및 객체 생성
    VideoWriter outputVideo("output_video.mp4", VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, frameSize);
    
    Point old(320, 45);
    int dist;
    int vel1=0,vel2=0,y=0;

    while (true) {
        gettimeofday(&start, NULL);
        Mat frame;
        cap >> frame;

        if (frame.empty())
            break;

        Mat cloneframe = frame.clone();
        Mat roi = frame(roiRect);

        // 그레이스케일
        Mat gray;
        cvtColor(roi, gray, COLOR_BGR2GRAY);

        // 노이즈 제거
        Mat denoised;
        GaussianBlur(gray, denoised, Size(5, 5), 0);

        // 밝기 보정
        Scalar meanValue = mean(denoised);
        double targetMean = 100.0;  // 평균 밝기 값
        double inputMean = meanValue[0];
        Mat corrected = denoised + (targetMean - inputMean);

        // 이진화
        Mat binary;
        threshold(corrected, binary, 230, 255, THRESH_BINARY | THRESH_OTSU);

        // 컬러 이미지로 변환
        Mat binaryColor;
        cvtColor(binary, binaryColor, COLOR_GRAY2BGR);

        // 이미지 복사
        Mat labeledImage = binary.clone();

        // 레이블링
        Mat stats, centroids;
        int numLabels = connectedComponentsWithStats(labeledImage, labeledImage, stats, centroids);

        Point closestPoint;
        int um = INT_MAX, x=1;
        int error;
        
        // 라인 후보 영역 찾기
        for (int i = 1; i < numLabels; i++) {
            Point center(stats.at<int>(i, CC_STAT_LEFT) + stats.at<int>(i, CC_STAT_WIDTH) / 2,
                stats.at<int>(i, CC_STAT_TOP) + stats.at<int>(i, CC_STAT_HEIGHT) / 2);
            dist = norm(center - old);
            if (um > dist) {
                um = dist;
                closestPoint = center;
                x = i;
            }
        }
        for (int i = 1; i < numLabels; i++) {
            if (x==i) {
                // 실제 라인
                rectangle(binaryColor, Point(stats.at<int>(i, CC_STAT_LEFT), stats.at<int>(i, CC_STAT_TOP)),
                    Point(stats.at<int>(i, CC_STAT_LEFT) + stats.at<int>(i, CC_STAT_WIDTH),
                        stats.at<int>(i, CC_STAT_TOP) + stats.at<int>(i, CC_STAT_HEIGHT)),
                    Scalar(0, 0, 255), 1);

                // 중심점
                Point center(stats.at<int>(i, CC_STAT_LEFT) + stats.at<int>(i, CC_STAT_WIDTH) / 2,
                    stats.at<int>(i, CC_STAT_TOP) + stats.at<int>(i, CC_STAT_HEIGHT) / 2);
                circle(binaryColor, center, 3, Scalar(0, 0, 255), -1);
                old = closestPoint;
            }
            else {
                // 노이즈
                rectangle(binaryColor, Point(stats.at<int>(i, CC_STAT_LEFT), stats.at<int>(i, CC_STAT_TOP)),
                    Point(stats.at<int>(i, CC_STAT_LEFT) + stats.at<int>(i, CC_STAT_WIDTH),
                        stats.at<int>(i, CC_STAT_TOP) + stats.at<int>(i, CC_STAT_HEIGHT)),
                    Scalar(255, 0, 0), 1);
                // 중심점
                Point center(stats.at<int>(i, CC_STAT_LEFT) + stats.at<int>(i, CC_STAT_WIDTH) / 2,
                    stats.at<int>(i, CC_STAT_TOP) + stats.at<int>(i, CC_STAT_HEIGHT) / 2);
                circle(binaryColor, center, 3, Scalar(255, 0, 0), -1);
            }
        }
        if (mx.kbhit()) //키보드입력 체크
            {
                char c = mx.getch(); //키입력 받기
                
                if(c =='q') break;
                else if(c=='s'){
                    vel1=200;
                    vel2=-200;
                    y++;
                }
            }
        error = old.x-320;
        if(error>0 && y>0)
            vel1 = 200 + 0.5 * error;
        else if(error<0 && y>0)
            vel2 = -200 + 0.5 * error;
        
        mx.setVelocity(vel1, vel2); //전진 속도명령 전송
        

        // 결과 프레임에 적용
        binaryColor.copyTo(roi);

        // 결과 영상에 프레임 저장
        outputVideo << frame;

        writer1 << cloneframe;
        writer2 << roi;

        if (ctrl_c_pressed) break; //Ctrl+c 누르면 탈출
        gettimeofday(&end1,NULL);
        diff1= end1.tv_sec+end1.tv_usec/1000000.0-start.tv_sec-start.tv_usec/1000000.0;
        cout << "time:" << diff1 << endl; //실행시간 출력
        cout << "error" << error <<endl;
        cout << "vel1: "<<vel1<<", vel2: "<<vel2<<endl;
    }

    mx.close(); //장치닫기

    // 비디오 객체 해제
    cap.release();
    outputVideo.release();

    destroyAllWindows();

    return 0;
}


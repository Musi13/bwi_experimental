
#include <std_msgs/String.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/image_encodings.h>

#include "bwi_scavenger/ColorShirt.h"
#include "ScavTaskColorShirt.h"

#include <boost/date_time/posix_time/posix_time.hpp>

#define INTMAX (32767)
#define INTMIN (-32767)
#define COLOR_RATIO (0.35)
#define DISTANCE_TO_COLOR (200)
#define SHIRT_HEIGHT_TOP (-0.1)
#define SHIRT_HEIGHT_BOTTOM (-0.9)

typedef pcl::PointCloud<pcl::PointXYZRGB> PointCloud;

enum Color{ BLUE, RED, GREEN, YELLOW, ORANGE }; 

sensor_msgs::ImageConstPtr image; 

std::string path_to_image; 

struct Rgb {
    float r; float g; float b;        
    Rgb() : r(), g(), b() {}
    Rgb( float rr, float gg, float bb ) : r(rr), g(gg), b(bb) {}
} baseline;


void callback_image_saver(const sensor_msgs::ImageConstPtr& msg) {
    image = msg;
} 

void callback_human_detection(const PointCloud::ConstPtr& msg)
{

    int color_cnt = 0; 

    switch (shirt_color) {
        case RED:       baseline = Rgb(255.0, 0.0, 0.0);    break;
        case BLUE:      baseline = Rgb(0.0, 0.0, 255.0);    break;
        case GREEN:     baseline = Rgb(0.0, 255.0, 0.0);    break;
        case YELLOW:    baseline = Rgb(255.0, 255.0, 0.0);  break; 
        case ORANGE:    baseline = Rgb(191.0, 87.0, 0.0);   break;
    }

    BOOST_FOREACH (const pcl::PointXYZRGB& pt, msg->points) {

        // here we assume the waist height is 90cm, and neck height is 160cm; 
        // the robot sensor's height is 60cm
        if (pt.y > SHIRT_HEIGHT_BOTTOM and pt.y < SHIRT_HEIGHT_TOP 
                and getColorDistance( &pt, &baseline) < DISTANCE_TO_COLOR) 
            color_cnt++;
    }

    float ratio = (float) color_cnt / (float) msg->points.size();

    ROS_INFO("ratio is %f", ratio);

    if (ratio > COLOR_RATIO && ros::ok()) { 

        ROS_INFO("person wearing %s shirt detected", shirt_color.c_str()); 
        
        boost::posix_time::ptime curr_time = boost::posix_time::second_clock::local_time();  
        std::string time_str = boost::posix_time::to_simple_string(curr_time); 

        cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(image, sensor_msgs::image_encodings::BGR8);
        path_to_image = path_to_directory + "color_shirt_" + time_str, cv_ptr->image; 
        cv::imwrite(path_to_image);

    }
}

void ScavTaskColorShirt::executeTask(int timeout, TaskResult &result, std::string &record)
{

    ros::Subscriber sub1 = nh->subscribe("/segbot_pcl_person_detector/human_clouds", 1, callback_human_detection);

    image_transport::ImageTransport it(*nh);
    image_transport::Subscriber sub = it.subscribe ("/nav_kinect/rgb/image_color", 1, callback_image_saver);

    ros::Duration(1.0).sleep();

    // 3 threads: receiving point cloud, receiving image, color-shirt service
    ros::AsyncSpinner spinner(2); 
    spinner.start(); 
    ros::waitForShutdown(); 

    record = path_to_image; 
    result = SUCCEEDED; 
}



#include "apriltag_rviz/apriltag_visual.h"

namespace apriltag_rviz {

using namespace apriltag_msgs;

ApriltagVisual::ApriltagVisual(Ogre::SceneManager* scene_manager,
                               Ogre::SceneNode* camera_node)
    : scene_manager_(scene_manager),
      tag_node_(camera_node->createChildSceneNode()),
      arrow_(new rviz::Arrow(scene_manager, tag_node_)),
      axes_(new rviz::Axes(scene_manager, tag_node_)) {
  // The state is unconfigured
  // TODO: hide arrow for now
}

ApriltagVisual::ApriltagVisual(Ogre::SceneManager* scene_manager,
                               Ogre::SceneNode* camera_node,
                               const apriltag_msgs::Apriltag& msg)
    : ApriltagVisual(scene_manager, camera_node) {}

ApriltagVisual::~ApriltagVisual() {
  scene_manager_->destroySceneNode(tag_node_);
}

void ApriltagVisual::setMessage(const Apriltag& msg) {
  // TODO: fix this hardcoded variable based on tag family
  int tag_bit = 6;

  // Move the tag node to the correct position with respect to the camera node
  const geometry_msgs::Pose& pose = msg.pose;
  Ogre::Vector3 position(pose.position.x, pose.position.y, pose.position.z);
  Ogre::Quaternion orientation(pose.orientation.w, pose.orientation.x,
                               pose.orientation.y, pose.orientation.z);
  tag_node_->setPosition(position);
  tag_node_->setOrientation(orientation);

  // TODO: the geometry of the axes and arrow will not change, maybe move this
  // to the constructor?
  // Update geometry of the axes
  const float tag_size = msg.size;
  axes_->set(tag_size, tag_size / (tag_bit + 2));
  // Update geometry of the arrow
  arrow_->set(tag_size, tag_size / (tag_bit + 2), tag_size / 4, tag_size / 5);
}

void ApriltagVisual::setColor(float r, float g, float b, float a) {
  // TODO: set Color using static property
}

/// ========================
/// ApriltagVisual::Property
/// ========================
ApriltagVisual::Property ApriltagVisual::property = ApriltagVisual::Property();

void ApriltagVisual::Property::setColor(const Ogre::ColourValue& color) {
  setColor(color.r, color.g, color.b);
}

void ApriltagVisual::Property::setColor(float r, float g, float b) {
  color[0] = r;
  color[1] = g;
  color[2] = b;
}

}  // namespace apriltag_rviz

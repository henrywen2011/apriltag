/* (C) 2013-2015, The Regents of The University of Michigan
All rights reserved.

This software may be available under alternative licensing
terms. Contact Edwin Olson, ebolson@umich.edu, for more information.

   Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
 */

#ifndef _APRILTAG_H
#define _APRILTAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "common/matd.h"
#include "common/image_u8.h"
#include "common/zarray.h"
#include "common/workerpool.h"
#include "common/timeprofile.h"
#include <pthread.h>

#define APRILTAG_TASKS_PER_THREAD_TARGET 10

struct quad
{
    float p[4][2]; // corners

    // H: tag coordinates ([-1,1] at the black corners) to pixels
    // Hinv: pixels to tag
    matd_t *H, *Hinv;
};

// Represents a tag family. Every tag belongs to a tag family. Tag
// families are generated by the Java tool
// april.tag.TagFamilyGenerator and can be converted to C using
// april.tag.TagToC.
typedef struct apriltag_family apriltag_family_t;
struct apriltag_family
{
    // How many codes are there in this tag family?
    uint32_t ncodes;

    // The codes in the family.
    uint64_t *codes;

    // how wide (in bit-sizes) is the black border? (usually 1)
    uint32_t black_border;

    // how many bits tall and wide is it? (e.g. 36bit tag ==> 6)
    uint32_t d;

    // minimum hamming distance between any two codes. (e.g. 36h11 => 11)
    uint32_t h;

    // a human-readable name, e.g., "tag36h11"
    char *name;

    // some implementations may preprocess codes in order to
    // accelerate decoding.  They put their data here. (Do not use the
    // same apriltag_family instance in more than one implementation)
    void *impl;
};


struct apriltag_quad_thresh_params
{
    // reject quads containing too few pixels
    int min_cluster_pixels;

    // how many corner candidates to consider when segmenting a group
    // of pixels into a quad.
    int max_nmaxima;

    // Reject quads where pairs of edges have angles that are close to
    // straight or close to 180 degrees. Zero means that no quads are
    // rejected. (In radians).
    float critical_rad;

    // When fitting lines to the contours, what is the maximum mean
    // squared error allowed?  This is useful in rejecting contours
    // that are far from being quad shaped; rejecting these quads "early"
    // saves expensive decoding processing.
    float max_line_fit_mse;

    // When we build our model of black & white pixels, we add an
    // extra check that the white model must be (overall) brighter
    // than the black model.  How much brighter? (in pixel values,
    // [0,255]). .
    int min_white_black_diff;

    // should the thresholded image be deglitched? This
    int deglitch;
};

// Represents a detector object. Upon creating a detector, all fields
// are set to reasonable values, but can be overridden by accessing
// these fields.
typedef struct apriltag_detector apriltag_detector_t;
struct apriltag_detector
{
    ///////////////////////////////////////////////////////////////
    // User-configurable parameters.

    // How many threads should be used?
    int nthreads;

    // detection of quads can be done on a lower-resolution image,
    // improving speed at a cost of pose accuracy and a slight
    // decrease in detection rate. Decoding the binary payload is
    // still done at full resolution. .
    float quad_decimate;

    // What Gaussian blur should be applied to the segmented image
    // (used for quad detection?)  Parameter is the standard deviation
    // in pixels.  Very noisy images benefit from non-zero values
    // (e.g. 0.8).
    float quad_sigma;

    // when non-zero, detections are refined in a way intended to
    // increase the number of detected tags. Especially effective for
    // very small tags near the resolution threshold (e.g. 10px on a
    // side).
    int refine_decode;

    // when non-zero, detections are refined in a way intended to
    // increase the accuracy of the extracted pose. This is done by
    // maximizing the contrast around the black and white border of
    // the tag. This generally increases the number of successfully
    // detected tags, though not as effectively (or quickly) as
    // refine_decode.
    //
    // This option must be enabled in order for "goodness" to be
    // computed.
    int refine_pose;

    // When non-zero, write a variety of debugging images to the
    // current working directory at various stages through the
    // detection process. (Somewhat slow).
    int debug;

    struct apriltag_quad_thresh_params qtp;

    ///////////////////////////////////////////////////////////////
    // Statistics relating to last processed frame
    timeprofile_t *tp;

    uint32_t nedges;
    uint32_t nsegments;
    uint32_t nquads;

    ///////////////////////////////////////////////////////////////
    // Internal variables below

    // Not freed on apriltag_destroy; a tag family can be shared
    // between multiple users. The user should ultimately destroy the
    // tag family passed into the constructor.
    zarray_t *tag_families;

    // Used to manage multi-threading.
    workerpool_t *wp;

    // Used for thread safety.
    pthread_mutex_t mutex;
};

// Represents the detection of a tag. These are returned to the user
// and must be individually destroyed by the user.
typedef struct apriltag_detection apriltag_detection_t;
struct apriltag_detection
{
    // a pointer for convenience. not freed by apriltag_detection_destroy.
    apriltag_family_t *family;

    // The decoded ID of the tag
    int id;

    // How many error bits were corrected? Note: accepting large numbers of
    // corrected errors leads to greatly increased false positive rates.
    // NOTE: As of this implementation, the detector cannot detect tags with
    // a hamming distance greater than 2.
    int hamming;

    // A measure of the quality of tag localization: measures the
    // average contrast of the pixels around the border of the
    // tag. refine_pose must be enabled, or this field will be zero.
    float goodness;

    // A measure of the quality of the binary decoding process: the
    // average difference between the intensity of a data bit versus
    // the decision threshold. Higher numbers roughly indicate better
    // decodes. This is a reasonable measure of detection accuracy
    // only for very small tags-- not effective for larger tags (where
    // we could have sampled anywhere within a bit cell and still
    // gotten a good detection.)
    float decision_margin;

    // The 3x3 homography matrix describing the projection from an
    // "ideal" tag (with corners at (-1,-1), (1,-1), (1,1), and (-1,
    // 1)) to pixels in the image. This matrix will be freed by
    // apriltag_detection_destroy.
    matd_t *H;

    // The center of the detection in image pixel coordinates.
    double c[2];

    // The corners of the tag in image pixel coordinates. These always
    // wrap counter-clock wise around the tag.
    double p[4][2];
};

// don't forget to add a family!
apriltag_detector_t *apriltag_detector_create();

// add a family to the apriltag detector. caller still "owns" the family.
// a single instance should only be provided to one apriltag detector instance.
void apriltag_detector_add_family(apriltag_detector_t *td, apriltag_family_t *fam);

// does not deallocate the family.
void apriltag_detector_remove_family(apriltag_detector_t *td, apriltag_family_t *fam);

// unregister all families, but does not deallocate the underlying tag family objects.
void apriltag_detector_clear_families(apriltag_detector_t *td);

// Destroy the april tag detector (but not the underlying
// apriltag_family_t used to initialize it.)
void apriltag_detector_destroy(apriltag_detector_t *td);

// Detect tags from an image and return an array of
// apriltag_detection_t*. You can use apriltag_detections_destroy to
// free the array and the detections it contains, or call
// _detection_destroy and zarray_destroy yourself.
zarray_t *apriltag_detector_detect(apriltag_detector_t *td, image_u8_t *im_orig);

// Call this method on each of the tags returned by apriltag_detector_detect
void apriltag_detection_destroy(apriltag_detection_t *det);

// destroys the array AND the detections within it.
void apriltag_detections_destroy(zarray_t *detections);

#ifdef __cplusplus
}
#endif

#endif

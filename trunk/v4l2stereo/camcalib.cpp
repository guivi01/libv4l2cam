/*
 Stereo camera calibration using OpenCV
 Functions for simple sparse stereo
 Copyright (C) 2010 Bob Mottram and Giacomo Spigler
 fuzzgun@gmail.com

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "camcalib.h"


enum {
    MINORU_VIEW_LEFT=0,
    MINORU_VIEW_RIGHT,
    MINORU_VIEW_INTERLACE,
    MINORU_VIEW_SIDEBYSIDE
};

camcalib::camcalib()
{
    double intCalib[] = {
        340,   0, 330,
          0, 340,  94,
          0,   0,   1
    };
    intrinsicCalibration_right = cvMat(3,3,CV_64F,intCalib);
    intrinsicCalibration_left = cvMat(3,3,CV_64F,intCalib);

    double trans[] = { -0.575, 0, 0 };
    extrinsicTranslation = cvMat(3,1,CV_64F, trans);

    disparityToDepth = cvMat(4,4,CV_64F);

    double eye3[] = {
          1,   0,   0,
          0,   1,   0,
          0,   0,   1
    };
    extrinsicRotation = cvMat(3,3,CV_64F, eye3);
}

camcalib::~camcalib()
{
}

CvMat* camcalib::matMul(const CvMat* A, const CvMat* B) 
{
    assert(A->cols == B->rows);

    CvMat* M = cvCreateMat(A->rows, B->cols, A->type);
    cvMatMul(A, B, M);
    return M;
}

void camcalib::editHomographyGUI(
    IplImage* leftim,
    IplImage* rightim,
    CvMat* hl,
    CvMat* hr)
{
    CvMat* H_L = cvCloneMat(hl);
    CvMat* H_R = cvCloneMat(hr);
    cvNamedWindow ("Homography", CV_WINDOW_AUTOSIZE);
    int v_max = leftim->height;
    int h_max = leftim->width;
    int v = v_max/2;
    int h = h_max/2;
    cvCreateTrackbar("v","Homography",&v,v_max,NULL);
    cvCreateTrackbar("h","Homography",&h,h_max,NULL);

    IplImage* render = cvCloneImage(leftim);
    int key = 0;
    while (key !='\x1b')  {
		
        cvWarpPerspective(leftim,render,hl);

        hl->data.db[2] = H_L->data.db[2]+h-h_max/2;
        hr->data.db[2] = H_R->data.db[2]+h-h_max/2;

	hl->data.db[5] = H_L->data.db[5]+v-v_max/2;
	hr->data.db[5] = H_R->data.db[5]+v-v_max/2;

	if (key == CVEX_KEY_ARROW_UP) {
            v--;
            cvSetTrackbarPos("v","Homography",v);
        }
        if (key == CVEX_KEY_ARROW_DOWN) {
            v++;
	    cvSetTrackbarPos("v","Homography",v);
        }
        if (key == CVEX_KEY_ARROW_LEFT) {
            h--;
            cvSetTrackbarPos("h","Homography",h);
        }
        if (key == CVEX_KEY_ARROW_RIGHT) {
            h++;
            cvSetTrackbarPos("h","Homography",h);
        }

        cvShowImage ("Homography", render);
        key = cvWaitKey(10) & 255;
        if( key == 27 ) break;
    }
    cvDestroyWindow("Homography");
    cvReleaseImage(&render);
    cvReleaseMat(&H_L);
    cvReleaseMat(&H_R);
}

// manually find the vertical offset
void camcalib::getShiftRectificationParameterGUI(
    IplImage* leftim,
    IplImage* rightim,
    int *_v)
{
    int v_max = leftim->height/5;
    int h_max = leftim->width;
    cvNamedWindow ("shift rectification", CV_WINDOW_AUTOSIZE);
    int v=*_v+v_max/2;

    cvCreateTrackbar("v","shift rectification",&v,v_max,NULL);
    int h = h_max/2;
    cvCreateTrackbar("h","shift rectification",&h,h_max,NULL);

    IplImage* render = cvCloneImage(leftim);
    int key = 0;
    while (key !='\x1b') {
        cvexWarpShift(rightim,render,h-h_max/2,v-v_max/2);
        cvAddWeighted(leftim,0.5,render,0.5,0,render);

        cvShowImage ("shift rectification", render);

        key = cvWaitKey(10) & 255;
        if( key == 27 ) break;
    }

    *_v=v-v_max/2;
    printf("Vertical Offset (vshift): %d\n", *_v);
    cvDestroyWindow("shift rectification");
    cvReleaseImage(&render);
}

// try to automatically find the vertical offset
void camcalib::getShiftRectificationParameter(
    IplImage* leftim,
    IplImage* rightim,
    int *_v)
{
    IplImage* rightim2 = cvCloneImage(rightim);
    unsigned char * leftim_ = (unsigned char*)leftim->imageData;
    unsigned char * rightim2_ = (unsigned char*)rightim2->imageData;
    int v_max = leftim->height/5;
    int h_max = leftim->width;
    int v=*_v+v_max/2,best_v=0;
    int h = h_max/2;
    int pixels = leftim->width * leftim->height * 3;
    int diff=0,min_diff=-1,p;

    for (h = 0; h < h_max; h++) {
        for (v = 0; v < v_max; v++) {
            cvexWarpShift(rightim,rightim2,h-h_max/2,v-v_max/2);
            diff = 0;
            for (int i = pixels-1; i >= 0; i--) {
                p = leftim_[i] - rightim2_[i];
                if (p < 0) p = -p;
                diff += p;
            }
            if ((diff < min_diff) || (min_diff==-1)) {
                min_diff = diff;
                best_v = v;
            }
        }
    }

    *_v=best_v-v_max/2;
    printf("Vertical Offset (vshift): %d\n", *_v);
    cvReleaseImage(&rightim2);
}

void camcalib::cvexMakeStereoImageSidebySide(
    IplImage* left,
    IplImage* right,
    IplImage* dest,
    int shift,
    int mode)
{
    IplImage* swap = cvCreateImage(cvGetSize(left),8,left->nChannels);
    IplImage* temp = cvCreateImage(cvGetSize(left),8,left->nChannels);
    cvexWarpShift(left,swap,-shift,0);
    cvexWarpShift(right,temp,+shift,0);

    IplImage* connect = cvexConnect(swap,temp,mode);

    cvResize(connect,dest);

    cvReleaseImage(&connect);
    cvReleaseImage(&swap);
    cvReleaseImage(&temp);
}

void camcalib::cvexMakeStereoImageInterlace(
    IplImage* left,
    IplImage* right,
    IplImage* dest,
    int shift)
{
    cvexWarpShift(left,dest,-shift,0);
    IplImage* swap = cvCreateImage(cvGetSize(right),8,3);
    cvexWarpShift(right,swap,+shift,0);
#pragma omp parallel for
    for(int j=0;j<left->height;j++) {
	if(j%2==1) {
            for(int i=0;i<left->width;i++) {
                dest->imageData[j*left->widthStep+i*3+0]=
                    swap->imageData[j*left->widthStep+i*3+0];
		dest->imageData[j*left->widthStep+i*3+1]=
                    swap->imageData[j*left->widthStep+i*3+1];
		dest->imageData[j*left->widthStep+i*3+2]=
                    swap->imageData[j*left->widthStep+i*3+2];
            }
        }
    }
    cvReleaseImage(&swap);
}

/* flip the given image so that the camera can be mounted upside down */
void camcalib::flip(
    int image_width,
    int image_height,
    unsigned char* raw_image,
    unsigned char* flipped_frame_buf)
{
    int max = image_width * image_height * 3;
    for (int i = 0; i < max; i += 3) {
        flipped_frame_buf[i] = raw_image[(max - 3 - i)];
        flipped_frame_buf[i + 1] = raw_image[(max - 3 - i + 1)];
        flipped_frame_buf[i + 2] = raw_image[(max - 3 - i + 2)];
    }
    memcpy(raw_image, flipped_frame_buf, max * sizeof(unsigned char));
}

void camcalib::stereo_camera_calibrate(
    int image_width,
    int image_height,
    int pattern_squares_x,
    int pattern_squares_y,
    int square_size_mm,
    std::string dev0,
    std::string dev1,
    bool flip_left_image,
    bool flip_right_image,
    int calibration_images,
    bool headless)
{
    int retval = system("mkdir calibration_image");
    retval = system("mkdir capture");
    retval = system("rm calibration_image/*");
    retval = system("rm capture/*");

    CvMat* hright = cvCreateMat(3,3,CV_64F);
    CvMat* hleft = cvCreateMat(3,3,CV_64F);
    CvMat* Q = cvCreateMat(4,4,CV_64F);
    cvSetIdentity(hleft);
    cvSetIdentity(hright);

    std::string window_title = "Stereo camera calibration";

    int w = image_width, h = image_height;

    Camera leftcam(dev0.c_str(), w, h, 30);
    Camera rightcam(dev1.c_str(), w, h, 30);

    IplImage *l=cvCreateImage(cvSize(w, h), 8, 3);
    unsigned char *l_=(unsigned char *)l->imageData;

    IplImage *r=cvCreateImage(cvSize(w, h), 8, 3);
    unsigned char *r_=(unsigned char *)r->imageData;
    unsigned char * temp_image = NULL;

    while (leftcam.Get()==0) usleep(100);

    leftcam.toIplImage(l);
    rightcam.toIplImage(r);

    if (flip_left_image) {
        if (temp_image == NULL) {
            temp_image = new unsigned char[w * h * 3];
        }
        flip(w, h, l_, temp_image);
    }

    IplImage* render = cvCloneImage(l);

    int mode = MINORU_VIEW_SIDEBYSIDE;
    int dis = render->width/2;
    if (!headless) {
        cvNamedWindow (window_title.c_str(), CV_WINDOW_AUTOSIZE);
        cvCreateTrackbar(
            "switch",window_title.c_str(),&mode,MINORU_VIEW_SIDEBYSIDE,NULL);
        cvCreateTrackbar(
            "parallax",window_title.c_str(),&dis,render->width,NULL);
    }

    int v_shift=0;
    int key=0;

    bool isRectification = false;

    CvexStereoCameraCalibration calib(
        cvGetSize(render),
        cvSize(pattern_squares_x, pattern_squares_y),
        square_size_mm);

    bool isLRFlip = false;

    int calibration_state = 0;

    while (key != '\x1b') {

        while ((leftcam.Get() == 0) ||
               (rightcam.Get() == 0)) {
            usleep(100);
        }

        leftcam.toIplImage(l);
        rightcam.toIplImage(r);

        if (flip_right_image) {
            if (temp_image == NULL) {
                temp_image = new unsigned char[w * h * 3];
            }
            flip(w, h, r_, temp_image);
        }

        if (flip_left_image) {
            if (temp_image == NULL) {
                temp_image = new unsigned char[w * h * 3];
            }
            flip(w, h, l_, temp_image);
        }

        if (key == 'r') {
            getShiftRectificationParameterGUI(l, r, &v_shift);
        }

        if (calibration_state == 2) {
            cvWarpPerspective(l, render,hleft);
            cvCopy(render, l);
            cvWarpPerspective(r, render,hright);
            cvCopy(render, r);
            if (!headless) {
                getShiftRectificationParameterGUI(l, r, &v_shift);
            }
            else {
                getShiftRectificationParameter(l, r, &v_shift);
            }
            calibration_state=3;
            break;
        }

        if (calibration_state == 0) {
            bool is = calib.findChess(l, r);
            if (is) {
                cvexSaveImage(
                    l, "calibration_image/left%03d.png",
                    calib.getImageCount());

                cvexSaveImage(
                    r, "calibration_image/right%03d.png",
                    calib.getImageCount());

                printf("Calibration image %d stored\n",
                    calib.getImageCount());
            }

            calib.drawChessboardCorners(
                l, l, CVEX_STEREO_CALIB_LEFT);

            calib.drawChessboardCorners(
                r, r, CVEX_STEREO_CALIB_RIGHT);

            if (is) {
                cvexSaveImage(
                    l, "calibration_image/left%03d_.png",
                    calib.getImageCount());
                cvexSaveImage(
                    r, "calibration_image/right%03d_.png",
                    calib.getImageCount());
            }
            if (calib.getImageCount() == calibration_images) {
                calibration_state=1;
            }
        }

        if (calibration_state == 1) {
            isRectification=true;

            printf("\nComputing intrinsic parameters...\n");
            calib.solveStereoParameter();
            calib.showIntrinsicParameters();

            printf("Computing rectification matrix...\n");
            calib.getRectificationMatrix(hleft, hright, Q);
            calib.showRectificationHomography();

            calib.showExtrinsicParameters();

            cout<<"Disparity to depth mapping matrix (Q):"<<endl;
            cvexShowMatrix(Q);

            calibration_state = 2;
        }

        if (key == 'h') {
            isRectification=true;
            editHomographyGUI(l, r, hleft, hright);
        }

        if (key == 'P') {
            isRectification=false;
        }

        if (key == 's') {
            static int saveCount=0;

            cvexSaveImage(
                l, "capture/capture_left%03d.png", saveCount);

            cvexSaveImage(
                r, "capture/capture_right%03d.png", saveCount);

            cvWarpPerspective(l, render, hleft);

            cvexSaveImage(
                render, "capture/capture_left_warp%03d.png", saveCount);

            cvWarpPerspective(r, render, hright);

            cvexSaveImage(
                render, "capture/capture_right_warp%03d.png", saveCount++);
        }

        if (key == 'F') {
            isLRFlip=(isLRFlip)?false:true;
        }

        if (key == CVEX_KEY_ARROW_LEFT) {
            mode--;
            cvSetTrackbarPos("switch", window_title.c_str(), mode);
        }

        if (key == CVEX_KEY_ARROW_RIGHT) {
            mode++;
            cvSetTrackbarPos("switch", window_title.c_str(), mode);
        }

        if (isRectification) {
            cvWarpPerspective(l, render, hleft);
            cvCopy(render, l);
            cvWarpPerspective(r, render, hright);
            cvCopy(render, r);
        }
        else {
            cvexWarpShift(r, r, 0, v_shift);
        }

        if (isLRFlip) {
            IplImage* swap;
            swap = l;
            l = r;
            r=swap;
        }

        switch(mode) {
            case MINORU_VIEW_LEFT: {
                cvCopy(l, render);
                break;
            }
            case MINORU_VIEW_RIGHT: {
                cvCopy(r, render);
                break;
            }
            case MINORU_VIEW_INTERLACE: {
                cvexMakeStereoImageInterlace(
                    l, r, render, dis-render->width/2);
                break;
            }
            case MINORU_VIEW_SIDEBYSIDE: {
                cvexMakeStereoImageSidebySide(
                    l, r, render, dis-render->width/2);
                break;
            }
        }

        if (!headless) cvShowImage (window_title.c_str(), render);
	
        key = cvWaitKey(10) & 255;
        if( key == 27 ) break;
    }

    // save results to file
    if (calibration_state == 3) {
        FILE * file;
        if ((file = fopen("calibration.txt","w"))) {
            calib.saveIntrinsicParameters(file);
            calib.saveRectificationHomography(file);
            calib.saveExtrinsicParameters(file);
            fprintf(file,"Disparity to depth mapping matrix (Q):\n");
            cvexSaveMatrix(Q, file);
            fprintf(file,"Vertical shift:\n%d\n",v_shift);
            fclose(file);
        }
    }

    cvReleaseMat (&hleft);
    cvReleaseMat (&hright);
    cvReleaseMat (&Q);
    cvReleaseImage (&render);
    cvDestroyWindow (window_title.c_str());

    /* destroy the left and right images */
    cvReleaseImage(&l);
    cvReleaseImage(&r);
    if (temp_image!=NULL) delete [] temp_image;
}


int camcalib::ParseIntrinsic(
    char * intrinsic_str,
    int camera_right)
{
    char str[256];
    double params[3*3];
    int i=0,index=0,p=0,success=0;
    while (intrinsic_str[i]!=0) {
        if ((index > 0) &&
            (intrinsic_str[i]==' ')) {
            str[index]=0;
            params[p++] = atof(str);
            index=0;   
        }
        else {
            str[index++] = intrinsic_str[i];
        }
        if (i==255) break;
        i++;
    }
    if (index > 0) {
        str[index]=0;
        params[p++] = atof(str);
    }
    if (p==9) {
        SetIntrinsic(params,camera_right);
        success=1;
    }
    return success;
}

int camcalib::ParseExtrinsicRotation(
    char * extrinsic_str)
{
    char str[256];
    double params[3*3];
    int i=0, index=0, p=0, success=0;
    while (extrinsic_str[i]!=0) {
        if ((index > 0) &&
            (extrinsic_str[i]==' ')) {
            str[index]=0;
            params[p++] = atof(str);
            index=0;   
        }
        else {
            str[index++] = extrinsic_str[i];
        }
        if (i==255) break;
        i++;
    }
    if (index > 0) {
        str[index]=0;
        params[p++] = atof(str);
    }
    if (p==9) {
        SetExtrinsicRotation(params);
        success=1;
    }
    return success;
}

int camcalib::ParseExtrinsicTranslation(
    char * extrinsic_str)
{
    char str[256];
    double params[3];
    int i=0,index=0,p=0,success=0;

    while (extrinsic_str[i]!=0) {
        if ((index > 0) &&
            (extrinsic_str[i]==' ')) {
            str[index]=0;
            params[p++] = atof(str);
            index=0;   
        }
        else {
            str[index++] = extrinsic_str[i];
        }
        if (i==255) break;
        i++;
    }
    if (index > 0) {
        str[index]=0;
        params[p++] = atof(str);
    }
    if (p==3) {
        SetExtrinsicTranslation(params);
        success=1;
    }
    return success;
}

int camcalib::ParseCalibrationParameters(
    char * calibration_str,
    int &pattern_squares_x,
    int &pattern_squares_y,
    int &square_size_mm)
{
    char str[256];
    int params[3];
    int i=0,index=0,p=0,success=0;

    while (calibration_str[i]!=0) {
        if ((index > 0) &&
            (calibration_str[i]==' ')) {
            str[index]=0;
            params[p++] = atoi(str);
            index=0;   
        }
        else {
            str[index++] = calibration_str[i];
        }
        if (i==255) break;
        i++;
    }
    if (index > 0) {
        str[index]=0;
        params[p++] = atoi(str);
    }
    if (p==3) {
        pattern_squares_x = params[0];
        pattern_squares_y = params[1];
        square_size_mm = params[2];
        success=1;
    }
    return success;
}

void camcalib::SetExtrinsicTranslation(
    double * extrinsic)
{
    extrinsicTranslation = cvMat(3,1,CV_64F,extrinsic);
}

void camcalib::SetDisparityToDepth(
    double * disparity_to_depth)
{
    disparityToDepth = cvMat(4,4,CV_64F,disparity_to_depth);
}

void camcalib::SetExtrinsicRotation(
    double * extrinsic)
{
    rotationMatrixFromEuler(
        extrinsic[0], extrinsic[1], extrinsic[2],
        &extrinsicRotation);
}

void camcalib::SetIntrinsic(
    double * intrinsic,
    int camera_right)
{
    if (camera_right==0) {
        intrinsicCalibration_left = cvMat(3,3,CV_64F,intrinsic);
    }
    else {
        intrinsicCalibration_right = cvMat(3,3,CV_64F,intrinsic);
    }
}

void camcalib::SetRectification(
    double * params,
    int camera_right)
{
    rectification[camera_right].Set(params);
}

int camcalib::ParseRectification(
    char * rectification_str,
    int camera_right)
{
    return rectification[camera_right].Parse(rectification_str);
}

void camcalib::rotationMatrixFromEuler(
    double xAngle, double yAngle, double zAngle,
    CvMat * rotation)
{
    double cx,cy,cz;
    double sx,sy,sz;
    CvMat* x = cvCreateMat(3,3,CV_64F);
    CvMat* y = cvCreateMat(3,3,CV_64F);
    CvMat* z = cvCreateMat(3,3,CV_64F);
    CvMat* tempArray = cvCreateMat(3,3,CV_64F);

    cx = cos(xAngle);
    cy = cos(yAngle);
    cz = cos(zAngle);

    sx = sin(xAngle);
    sy = sin(yAngle);
    sz = sin(zAngle);

    cvmSet(x,0,0,1);   cvmSet(x,0,1,0);   cvmSet(x,0,2,0);
    cvmSet(x,1,0,0);   cvmSet(x,1,1,cx);  cvmSet(x,1,2,-sx);
    cvmSet(x,2,0,0);   cvmSet(x,2,1,sx);  cvmSet(x,2,2,cx);

    cvmSet(y,0,0,cy);  cvmSet(y,0,1,0);   cvmSet(y,0,2,sy);
    cvmSet(y,1,0,0);   cvmSet(y,1,1,1);   cvmSet(y,1,2,0);
    cvmSet(y,2,0,-sy); cvmSet(y,2,1,0);   cvmSet(y,2,2,cy);

    cvmSet(z,0,0,cz);  cvmSet(z,0,1,-sz); cvmSet(z,0,2,0);
    cvmSet(z,1,0,sz);  cvmSet(z,1,1,cz);  cvmSet(z,1,2,0);
    cvmSet(z,2,0,0);   cvmSet(z,2,1,0);   cvmSet(z,2,2,1);

    CvMat * m = matMul(z,y);
    tempArray = matMul(m,x);
    cvCopy(tempArray,rotation,0);

    cvReleaseMat(&tempArray);
    cvReleaseMat(&m);
    cvReleaseMat(&x);
    cvReleaseMat(&y);
    cvReleaseMat(&z);
}

void camcalib::RectifyImage(
    int right_image,
    int image_width,
    int image_height,
    unsigned char * image_data,
    int v_shift)
{
    rectification[right_image].update(
      image_width, image_height, image_data, v_shift);
}

void camcalib::RectifyImage(
    int right_image,
    int image_width,
    int image_height,
    int16_t *image_data,
    int v_shift)
{
    rectification[right_image].update(
        image_width, image_height, image_data, v_shift);
}

/* there might be better ways to do this */
std::string camcalib::lowercase(
    std::string str)
{
    for (int i=0; i < (int)str.size(); i++)
        str[i] = tolower(str[i]);
    return str;
}

/*
 This is intended to provide an easy way to set camera parameters for
 a few commonly used devices
 */
void camcalib::SetStereoCamera(
    std::string camera_type)
{
    camera_type = lowercase(camera_type);
    if (camera_type == "stereocar") {
        double intrinsic_left_stereocar[] = {
            271.87706,  0.00000,  165.52481,
            0.00000,  265.73705,  123.28512,
            0.00000,  0.00000,  1.00000  
        };
        double intrinsic_right_stereocar[] = {
            261.06584,  0.00000,  166.30977,
            0.00000,  257.98369,  118.05681,
            0.00000,  0.00000,  1.00000
        };
        double rectification_left_stereocar[] = {
            0.91800,  0.00116,  -2.96307,
           -0.02529,  0.93648,  16.08442,
           -0.00007,  -0.00011,  1.02434
        };
        double rectification_right_stereocar[] = {
            0.90266,  -0.00992,  21.32725,
           -0.01948,  0.99164,  -1.03703,
           -0.00041,  0.00012,  1.04763
        };
        double extrinsic_rotation_stereocar[] = {
            -0.05557,  -0.08680,  -0.04839
        };
        double extrinsic_translation_stereocar[] = {
            -572.27242/1000.0,  15.67428/1000.0,  -61.35427/1000.0
        };
        SetIntrinsic(intrinsic_left_stereocar,0);
        SetIntrinsic(intrinsic_right_stereocar,1);
        SetRectification(rectification_left_stereocar,0);
        SetRectification(rectification_right_stereocar,1);
        SetExtrinsicTranslation(extrinsic_translation_stereocar);
        SetExtrinsicRotation(extrinsic_rotation_stereocar);
    }
    if (camera_type == "minoru") {
        double intrinsic_left_minoru[] = {
            426.17406,  0.00000,  161.17516,
            0.00000,  422.45229,  130.79453,
            0.00000,  0.00000,  1.00000
        };
        double intrinsic_right_minoru[] = {
            402.84650,  0.00000,  178.72909,
            0.00000,  399.25476,  138.17671,
            0.00000,  0.00000,  1.00000
        };
        double rectification_left_minoru[] = {
            1.07943,  -0.02260,  4.38514,
           -0.07749,  1.16865,  -16.40799,
           -0.00077,  0.00003,  1.06500
        };
        double rectification_right_minoru[] = {
            1.14566,  -0.03051,  -29.29670,
           -0.08014,  1.23097,  -23.07482,
           -0.00080,  -0.00001,  1.09112
        };
        double extrinsic_rotation_minoru[] = {
            0.01717,  0.00861, 0.00074
        };
        double extrinsic_translation_minoru[] = {
            -58.77396/1000.0,  1.52852/1000.0,  -19.96079/1000.0
        };
        double disparity_to_depth_minoru[] = {
            1.00000,  0.00000,  0.00000,  -13.73735,
            0.00000,  1.00000,  0.00000,  -133.80054,
            0.00000,  0.00000,  0.00000,  492.23168,
            0.00000,  0.00000,  -0.01704,  -0.00000
        };
        SetIntrinsic(intrinsic_left_minoru,0);
        SetIntrinsic(intrinsic_right_minoru,1);
        SetRectification(rectification_left_minoru,0);
        SetRectification(rectification_right_minoru,1);
        SetExtrinsicTranslation(extrinsic_translation_minoru);
        SetExtrinsicRotation(extrinsic_rotation_minoru);
        SetDisparityToDepth(disparity_to_depth_minoru);
    }
    if (camera_type == "c1") {
        double intrinsic_left_c1[] = {
            282.20404,  0.00000,  161.71900,
            0.00000,  276.41432,  128.57618,
            0.00000,  0.00000,  1.00000
        };
        double intrinsic_right_c1[] = {
            279.43787,  0.00000,  149.43279,
            0.00000,  273.16425,  109.19415,
            0.00000,  0.00000,  1.00000
        };
        double rectification_left_c1[] = {
            1.20475,  -0.07873,  -37.60478,
            0.04822,  1.27256,  -54.37895,
           -0.00028,  0.00003,  1.03804
        };
        double rectification_right_c1[] = {
            1.18951,  -0.10257,  -3.97842,
            0.04454,  1.28148,  -24.42923,
           -0.00045,  -0.00001,  1.06035
        };
        double extrinsic_rotation_c1[] = {
            0.01283,  -0.04696,  -0.01269
        };
        double extrinsic_translation_c1[] = {
            -116.55134/1000.0,  9.32135/1000.0,  -14.83672/1000.0
        };
        double disparity_to_depth_c1[] = {
            1.00000,  0.00000,  0.00000,  -119.29831,
            0.00000,  1.00000,  0.00000,  -118.93191,
            0.00000,  0.00000,  0.00000,  351.38281,
            0.00000,  0.00000,  -0.00855,  -0.00000
        };
        SetIntrinsic(intrinsic_left_c1,0);
        SetIntrinsic(intrinsic_right_c1,1);
        SetRectification(rectification_left_c1,0);
        SetRectification(rectification_right_c1,1);
        SetExtrinsicTranslation(extrinsic_translation_c1);
        SetExtrinsicRotation(extrinsic_rotation_c1);
        SetDisparityToDepth(disparity_to_depth_c1);
    }
}

// Digits recognition for SICK Robot Day 2010
#include <string> 
#include <iostream>
#include <vector>
#include <cv.h>
#include <highgui.h>
#include <algorithm>

const int THRESHOLD = 80; // was 100 parameter of the image thresholding
const int THRESHOLD_STEP = 20; // test +/- THRESHOLD

int findIndex( CvSeq *contour, CvPoint *pt )
{
  int i;
  for(i = 0; i < contour->total; i++)
  {
    CvPoint p = *CV_GET_SEQ_ELEM( CvPoint, contour, i );
    if( p.x == pt->x && p.y == pt->y ) 
      return i;
  }
  return -1; // not found
}

int clasifyDigit( CvSeq *contour, IplImage *debugImg=NULL )
// return -1 for unknown
{
  double area = cvContourArea( contour );
  if( area < 100 )
    return -1;

  CvRect rect = cvBoundingRect( contour );
  if( rect.height > 4*rect.width )
    return -1;
  if( rect.height < rect.width )
    return -1;

  if( rect.height < 60 )
    return -1; // look only for large numbers

  // verify number border (in Germany only)
  if( contour->v_prev )
  {
    CvMemStorage* tmpStorage = cvCreateMemStorage(0);
    CvSeq* frameShape = cvApproxPoly( contour->v_prev, sizeof(CvContour), tmpStorage,
                CV_POLY_APPROX_DP, cvContourPerimeter(contour->v_prev)*0.02, 0 );
    if( debugImg && frameShape->total == 4 )
      cvDrawContours( debugImg, frameShape, CV_RGB(255,0,0), CV_RGB(0,255,0), 0, 1, CV_AA, cvPoint(0,0) );
    int total = frameShape->total;
    cvReleaseMemStorage( &tmpStorage );
    if( total != 4 ) // rectangle
      return -1;
  }
  else
    return -1; // there is nothing containing this frame


  CvMoments moments;
  cvMoments( contour, &moments );
  double length = cvArcLength( contour );

  if ( contour->v_next )
  {
    // interior in pixels + has hole(s)
    if( contour->v_next->h_next )
    // interior in pixels + has 2 holes -> 8
    {
      if( contour->v_next->h_next->h_next )
        return -1; // 3 and more holes

      double subarea1 = fabs(cvContourArea( contour->v_next ));
      double subarea2 = fabs(cvContourArea( contour->v_next->h_next ));
      double frac1 = subarea1/area;
      double frac2 = subarea2/area;

      CvRect rect1 = cvBoundingRect(contour->v_next);
      CvRect rect2 = cvBoundingRect(contour->v_next->h_next);
      if( rect1.y > rect2.y )
      {
        double tmp = frac1; frac1 = frac2; frac2 = tmp;
        CvRect rtmp = rect1; rect1 = rect2; rect2 = rtmp;
      }

      if( frac1 > 0.1 && frac1 < 0.2 && frac2 > 0.2 && frac2 < 0.3 )
      {
        if( rect1.y + rect1.height < rect2.y )
        {
          if( rect1.width <= rect2.width && rect1.height <= rect2.height )
          {
            if( rect1.x >= rect2.x && rect1.x + rect1.width <= rect2.x + rect2.width )
            {
              return 8;
            }
          }
        }
      }
      return -1;
    }
    else
    {
      // reference values 0.203974, 0.310097/0.319935, 0.489391
      double subarea = fabs(cvContourArea( contour->v_next ));
      double frac = subarea/area;
      CvRect subRect = cvBoundingRect(contour->v_next);
//      printf( "subarea fraction: %lf\n", frac );
      if( frac < 0.25 )
      {
        CvMemStorage* tmpStorage = cvCreateMemStorage(0);
        CvSeq* holeShape = cvApproxPoly( contour->v_next, sizeof(CvContour), tmpStorage,
                    CV_POLY_APPROX_DP, cvContourPerimeter(contour->v_next)*0.02, 0 );
        int total = holeShape->total;
        cvReleaseMemStorage( &tmpStorage );
        if( total == 3 ) // triangular hole
          return 4;
        return -1;
      }
      else
      {
        if( subRect.height > rect.height/2 )
          return 0;

        // 6 and 9
        CvMoments holeMoments;
        cvMoments( contour->v_next, &holeMoments );
        if( moments.m01/moments.m00 < holeMoments.m01/holeMoments.m00 )
        {
          // hole has bigger Y (BEWARE OF SWAPPED Y-Coordinates!!!
          if( subRect.y > rect.y + rect.height/3 ) // 1/2 is too close
            return 6;
          else
            return -1;
        }
        else
        {
          if( subRect.y+subRect.height < rect.y + 2*rect.height/3 ) // 1/2 is too close
            return 9;
          else
            return -1;
        }
      }
    }
  }

  // no holes
  int i;
  CvSeq *hull = cvConvexHull2( contour );
  CvSeq *defects = cvConvexityDefects( contour, hull );
  if( defects->total < 2 )
    return -1;

  CvConvexityDefect best = *CV_GET_SEQ_ELEM( CvConvexityDefect, defects, 0 );
  int bestIndex = 0, secondIndex = 0;
  for( i = 1; i < defects->total; i++ )
  {
    CvConvexityDefect d = *CV_GET_SEQ_ELEM( CvConvexityDefect, defects, i );
    if( d.depth > best.depth )
    {
      best = d;
      bestIndex = i;
    }
  }
  CvConvexityDefect second = *CV_GET_SEQ_ELEM( CvConvexityDefect, defects, bestIndex==0?1:0 );
  for( i = 0; i < defects->total; i++ )
  {
    CvConvexityDefect d = *CV_GET_SEQ_ELEM( CvConvexityDefect, defects, i );
    if( i != bestIndex && d.depth > second.depth )
    {
      second = d;
      secondIndex = i;
    }
  }

  CvSlice slice = cvSlice( findIndex( contour, best.start ), findIndex( contour, best.end ) );  
  double arcLength = cvArcLength( contour, slice );

  int x,y;
  x = (best.start->x + best.end->x)/2 - (int)(moments.m10/moments.m00);
  y = (best.start->y + best.end->y)/2 - (int)(moments.m01/moments.m00);
  double entryLength = sqrt( (double)(best.start->x - best.end->x)*(best.start->x - best.end->x) 
                                   + (best.start->y - best.end->y)*(best.start->y - best.end->y) );

  if( best.depth > 2 * second.depth && arcLength > 0.4 * length && x < 0 && arcLength > 4* entryLength )
    return 3;

  if( best.depth > 8 * second.depth && x < 0 )
    return 1;

  if( best.depth < 1.1 * second.depth && best.depth > rect.width/2.0)
    return 2;

  if( arcLength > 5.0 * entryLength && y > 0 )
    return 5;

  if( best.depth > 5 * second.depth && x < 0 )
    return 7;

  return -1;
}

struct SmallerArea : public std::binary_function<CvRect, CvRect, bool> 
{
  bool operator()(const CvRect& left, const CvRect& right) const 
  { return left.height * left.width < right.height * right.width; }     
};


bool isItTarget( CvSeq *contour, IplImage *debugImg=NULL )
{
  if ( contour->v_next )
  {
    double area = fabs( cvContourArea( contour ));
    double minSubarea = 4000.0;
    double subarea;
    // it has hole(s)
    CvSeq* p = contour->v_next;
    unsigned int i;
    for( i = 0; p != NULL; i++ )
    {
      subarea = fabs( cvContourArea( p ));
      if( subarea < minSubarea )
        minSubarea = subarea;
      p = p->h_next;
    }
    if( i == 12 )
    {
      if( area < 15000.0 && minSubarea > 0.0 )
      {
        std::vector<CvRect> bb;
        p = contour->v_next;
        for( i = 0; p != NULL; i++ )
        {
          bb.push_back( cvBoundingRect( p ) );
          p = p->h_next;
        }
        std::sort( bb.begin(), bb.end(), SmallerArea() );

        unsigned int best, last;
        
        for( last = 8; last <= 12; last += 4 )
        {
          std::vector<unsigned int> bestArr;

          best = 0;
          for( i = 0; i < last; i++ )
          {
            if( bb[i].x + bb[i].y < bb[best].x + bb[best].y )
              best = i;
          }
          if( best < last - 4 )
            return false;
          bestArr.push_back( best );
          
          best = 0;
          for( i = 0; i < last; i++ )
          {
            if( bb[i].x + bb[i].y + bb[i].height + bb[i].width > bb[best].x + bb[best].y + bb[best].height + bb[best].width )
              best = i;
          }
          if( best < last - 4 )
            return false;
          bestArr.push_back( best );

          best = 0;
          for( i = 0; i < last; i++ )
          {
            if( bb[i].x - bb[i].y - bb[i].height < bb[best].x - bb[best].y - bb[best].height )
              best = i;
          }
          if( best < last - 4 )
            return false;
          bestArr.push_back( best );
          
          best = 0;
          for( i = 0; i < last; i++ )
          {
            if( bb[i].x + bb[i].width - bb[i].y > bb[best].x + bb[best].width - bb[best].y )
              best = i;
          }
          if( best < last - 4 )
            return false;
          bestArr.push_back( best );

          // now shold be bestArr uniqe permutation last-4, last-3, last-2, last-1
          std::sort( bestArr.begin(), bestArr.end() );
          for( i = 0; i < bestArr.size(); i++ )
          {
            if( bestArr[i] != last - 4 + i )
              return false;
          }
        }

        return true;
      }
    }
  }
  return false;
}

bool validDigitPosition( CvRect rect )
{
  double v = 0.6*rect.y + 0.8*rect.height - 144.0;
  return fabs(v) < 20.0;
}


int findDigit( const char *filename, char *outFilename = 0 )
{
  IplImage* img = cvLoadImage(filename);
  if(img == NULL)
  {
    std::cerr << "Failed to load the image: " << filename << std::endl;
    return 2;
  }

  IplImage* gray = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
  cvCvtColor(img, gray, CV_BGR2GRAY);
  IplImage* bin = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
  CvMemStorage* storage = cvCreateMemStorage(0);

  // erosion
  // IplConvKernel* cvCreateStructuringElementEx(int cols, int rows, int anchor_x, int anchor_y, int shape, int* values=NULL )
  //IplConvKernel* element = cvCreateStructuringElementEx( 3, 3, 0, 0, cv::MORPH_RECT );
  // void cvErode(const CvArr* src, CvArr* dst, IplConvKernel* element=NULL, int iterations=1)
  cvErode( gray, gray ); // element � structuring element used for erosion; if element=Mat() , a 3 x 3 rectangular structuring element is used.

  if( outFilename )
  {
    // extra computation, so it is done only once
    cvThreshold(gray, bin, THRESHOLD, 255, CV_THRESH_BINARY);
    cvCvtColor(bin, img, CV_GRAY2BGR);
  }

//  CvRect roi = cvRect(110, 49, 400, 280);
  CvRect roi = cvRect(0, 0, 640, 512);
  cvSetImageROI( bin, roi ); // try faster processing
  cvSetImageROI( gray, roi ); // try faster processing

  fprintf( stdout, "[" );
  for( int threshold = THRESHOLD-THRESHOLD_STEP; threshold <= THRESHOLD+THRESHOLD_STEP; threshold += THRESHOLD_STEP )
  {
  cvThreshold(gray, bin, threshold, 255, CV_THRESH_BINARY);

  CvSeq* contours = 0;
  int levels = 3;

  cvFindContours( bin, storage, &contours, sizeof(CvContour),
                    CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );
  //cvFindContours( bin, storage, &contours, sizeof(CvContour),
  //                  CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );  
  

  CvSeq* ptr = contours;
  std::vector< CvSeq* > stack;
  stack.push_back( ptr );
  fprintf( stdout, "(" );
  while( !stack.empty() )
  {
    ptr = stack.back();
    stack.pop_back();

    while( ptr )
    {
      if( isItTarget( ptr ) )
      {
        CvRect rect = cvBoundingRect( ptr ); // Centroid would be better
        fprintf( stdout, "('X', (%d,%d,%d,%d)),", rect.x+roi.x, rect.y+roi.y, rect.width, rect.height );
        if( outFilename )
        {
          cvDrawContours( img, ptr, CV_RGB(0,0,255), CV_RGB(0,255,0), -1, 1, CV_AA, cvPoint(roi.x,roi.y) );
        }
      }

      int digit = clasifyDigit( ptr );
      if( digit != -1 )
      {
        CvRect rect = cvBoundingRect( ptr );
        if( validDigitPosition( rect ) )
        {
          fprintf( stdout, "(%d, (%d,%d,%d,%d)),", digit, rect.x+roi.x, rect.y+roi.y, rect.width, rect.height );

          if( outFilename )
          {
            CvMoments moments;
            cvMoments( ptr, &moments );
            cvDrawContours( img, ptr, CV_RGB(255,0,0), CV_RGB(0,255,0), -1, 1, CV_AA, cvPoint(roi.x,roi.y) );
            CvPoint c = cvPoint((int)(moments.m10/moments.m00)+roi.x, (int)(moments.m01/moments.m00)+roi.y);
            cvCircle(img, c, 5, CV_RGB(255, 255, 0), 3);

            CvFont font;
            cvInitFont( &font, CV_FONT_HERSHEY_DUPLEX, 1.0, 1.0 );
            char buf[256];
            sprintf( buf, " %d", digit );
            c.x += (threshold-THRESHOLD)/THRESHOLD_STEP;
            c.y += (threshold-THRESHOLD)/THRESHOLD_STEP;
            cvPutText( img, buf, c, &font, CV_RGB(255,0,0));
          }
        }
      }

      if( ptr->v_next )
        stack.push_back( ptr->v_next );
      ptr = ptr->h_next;
    }
  }

  fprintf( stdout, ")," );
  }
  fprintf( stdout, "]\n" );
  fflush( stdout );

  if( outFilename )
    cvSaveImage(outFilename, img);

  cvReleaseMemStorage( &storage );
  cvReleaseImage(&bin);
  cvReleaseImage(&gray);
  cvReleaseImage(&img);
  return 0;
} 

const char* COMMAND_QUIT = "quit";
const char* COMMAND_FILE = "file";

int main( int argc, char *argv[] )
{
  if( argc > 2 )
  {
    // test single image
    findDigit( argv[1], argv[2] );
    return 0;
  }

  std::string command, subcommand;
  std::string filename;
  while(std::cin)
  {
    std::cin >> command;

    if(command == COMMAND_QUIT || !std::cin)
    {
      break;
    }
    else if(command == COMMAND_FILE)
    {
      std::cin.ignore(64, ' '); //skip the spaces
      std::getline(std::cin, filename);

      findDigit( filename.c_str() );
    }
    else
    {
      std::cerr << "Unknown command: " << command << std::endl;
    }
  }
  return 0;
}

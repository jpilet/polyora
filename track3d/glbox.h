/*  This file is part of Polyora, a multi-target tracking library.
    Copyright (C) 2010 Julien Pilet

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program. If not, see <http://www.gnu.org/licenses/>.

    To contact the author of this program, please send an e-mail to:
    julien.pilet(at)calodox.org
*/

/****************************************************************************
**
** This is a simple QGLWidget displaying an openGL wireframe box
**
****************************************************************************/

#ifndef GLBOX_H
#define GLBOX_H

#ifdef WIN32
//#include <GL/glew.h>
#endif
#include <QtOpenGL>
#include <cv.h>

#ifdef WITH_LIBVISION
#include <image.h>
#endif

//! Image + OpenGL widget. 
class GLBox : public QGLWidget
{
    Q_OBJECT

public:

    GLBox(QWidget* parent=0, const char* name=0, const QGLWidget* shareWidget=0);
    GLBox(const QGLFormat &format, QWidget *parent=0, const char *name=0, const QGLWidget* shareWidget=0);
    ~GLBox();

    void screenToImageCoord(int sx, int sy, float *imx, float *imy) const;
    void imageToScreenCoord(float x, float y, int *sx, int *sy) const;

    bool reloadImage;
    bool allowCache;
    /*! translate image of (dx,dy). Use 0,0 for normal centered rendering
     *  (default). dx points toward right, dy points up. Dx, dy are expressed in pixels.
     */ 
    void translate(float dx, float dy) { this->dx=dx, this->dy=dy; setupTransform();}
    void zoom(float sx, float sy) { this->sx=sx; this->sy=sy; setupTransform(); }
    void setAxisUp(bool up) { yAxisUp = up; setupTransform(); }

    void setImageSpace();

    void setImage(IplImage *image );

    /* Generate an OpenGL texture and return its ID. Take care of the OpenGL
     * context and call makeCurrent() if necessary.
     * \param texWidth width of the texture. Has to be a power of 2.
     * \param texHeight height of the texture. Has to be a power of 2.
     * \param smooth use GL_LINEAR (true) or GL_NEAREST (false)
     */
    static GLuint genGlTexture(int texWidth, int texHeight, bool smooth = false);

    /* Load an IplImage in an OpenGL texture.
     */
    void loadGlTexture(IplImage *im);
    
    
#ifdef WITH_LIBVISION
    void setImage(VL::RasterImage *im); // libvision support
#endif

    /*! off screen rendering.
     * create a GL context, call initializeGL() and paintGL(), and save the
     * result to the PNG or JPG file specified as argument.
     * \param w image width. If 0, uses the current widget width.
     * \param h image height. If 0, uses the current widget height.
     * \return true on success, false on error.
     */
    bool renderAndSave(const char *filename, int w=720, int h=576);

    float dx,dy;
    float sx,sy;
    void setupTransform();
    bool yAxisUp;

    bool smooth;

protected:

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL( int w, int h );

    // provide zoom and scroll control.
    virtual void mouseMoveEvent ( QMouseEvent * e );
    virtual void wheelEvent( QWheelEvent *e );
    virtual void mousePressEvent(QMouseEvent *e);

    // called by renderAndSave. These method should backup textures.
    virtual void saveContext();
    virtual void restoreContext();

    void loadTexture();

    IplImage *image;
    GLuint texture, savedTexture;
    int textureWidth, textureHeight;

#ifdef WITH_LIBVISION
    IplImage iplIm;
#endif

    float affineTransf[2][3];
    float invAffineTransf[2][3];

    void invertAffine();

private:
    void init(); // called by constructors
    QPoint lastMousePos;
};

#ifdef WITH_LIBVISION
void iplShareRaster(VL::RasterImage *ri, IplImage *ipl);
#endif

#endif // GLBOX_H

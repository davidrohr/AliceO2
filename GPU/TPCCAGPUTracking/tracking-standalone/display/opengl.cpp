#include "AliHLTTPCCADef.h"
#include "opengl_backend.h"

#include <vector>
#include <array>
#include <tuple>

#ifndef R__WIN32
#include "bitmapfile.h"
#endif

#include "AliHLTTPCCASliceData.h"
#include "AliHLTTPCCAStandaloneFramework.h"
#include "AliHLTTPCCATrack.h"
#include "AliHLTTPCCATracker.h"
#include "AliHLTTPCCATrackerFramework.h"
#include "AliHLTTPCGMMergedTrack.h"
#include "AliHLTTPCGMPropagator.h"
#include "include.h"
#include "../cmodules/timer.h"
#include "../cmodules/qconfig.h"

struct DrawArraysIndirectCommand
{
	DrawArraysIndirectCommand(uint a = 0, uint b = 0, uint c = 0, uint d = 0) : count(a), instanceCount(b), first(c), baseInstance(d) {}
	uint  count;
	uint  instanceCount;
	uint  first;
	uint  baseInstance;
};

//#define CHKERR(cmd) {cmd;}
#define CHKERR(cmd) {(cmd); GLenum err = glGetError(); while (err != GL_NO_ERROR) {printf("OpenGL Error %d: %s (%s: %d)\n", err, gluErrorString(err), __FILE__, __LINE__);exit(1);}}

#define OPENGL_EMULATE_MULTI_DRAW 0

bool smoothPoints = true;
bool smoothLines = false;
bool depthBuffer = false;
const int drawQualityPoint = 0;
const int drawQualityLine = 0;
const int drawQualityPerspective = 0;
int drawQualityMSAA = 0;
int drawQualityVSync = 0;
bool useGLIndirectDraw = true;

bool camLookOrigin = false;
bool camYUp = false;
float angleRollOrigin = -1e9;
int cameraMode = 0;

float maxClusterZ = 0;

#define fgkNSlices 36
#ifndef BUILD_QA
bool SuppressHit(int iHit) {return false;}
int GetMCLabel(int track) {return(-1);}
#endif
volatile int needUpdate = 0;
void ShowNextEvent() {needUpdate = 1;}
#define GL_SCALE_FACTOR 100.f
int screenshot_scale = 1;

const int init_width = 1024, init_height = 768;
int screen_width = init_width, screen_height = init_height;

GLuint vbo_id, indirect_id;
int indirectSliceOffset[fgkNSlices];
typedef std::tuple<GLsizei, GLsizei, int> vboList;
struct GLvertex {GLfloat x, y, z; GLvertex(GLfloat a, GLfloat b, GLfloat c) : x(a), y(b), z(c) {}};
std::vector<GLvertex> vertexBuffer[fgkNSlices];
std::vector<GLint> vertexBufferStart[fgkNSlices];
std::vector<GLsizei> vertexBufferCount[fgkNSlices];
int drawCalls = 0;
inline void drawVertices(const vboList& v, const GLenum t)
{
	auto first = std::get<0>(v);
	auto count = std::get<1>(v);
	auto iSlice = std::get<2>(v);
	if (count == 0) return;
	drawCalls += count;

	if (useGLIndirectDraw)
	{
		CHKERR(glMultiDrawArraysIndirect(t, (void*) (size_t) ((indirectSliceOffset[iSlice] + first) * sizeof(DrawArraysIndirectCommand)), count, 0));
	}
	else if (OPENGL_EMULATE_MULTI_DRAW)
	{
		for (int k = 0;k < count;k++) CHKERR(glDrawArrays(t, vertexBufferStart[iSlice][first + k], vertexBufferCount[iSlice][first + k]));
	}
	else
	{
		CHKERR(glMultiDrawArrays(t, vertexBufferStart[iSlice].data() + first, vertexBufferCount[iSlice].data() + first, count));
	}

	//CHKERR(glDrawArrays(t, vertexBufferStart[iSlice][first], vertexBufferStart[iSlice][first + count - 1] - vertexBufferStart[iSlice][first] + vertexBufferCount[iSlice][first + count - 1])); //TEST, combine in single strip
	
	/*fprintf(stderr, "Draw start %d count %d: %d (size %lld)\n", (vertexBufferStart.data() + v.first)[0], (vertexBufferCount.data() + v.first)[0], v.second, (long long int) vertexBuffer.size());
	for (int k = vertexBufferStart.data()[v.first];k < vertexBufferStart.data()[v.first + v.second - 1] + vertexBufferCount.data()[v.first + v.second - 1];k++)
	{
		printf("Vertex %f %f %f\n", vertexBuffer[k].x, vertexBuffer[k].y, vertexBuffer[k].z);
	}*/
}
inline void insertVertexList(int iSlice, size_t first, size_t last)
{
	if (first == last) return;
	vertexBufferStart[iSlice].emplace_back(first);
	vertexBufferCount[iSlice].emplace_back(last - first);
}

bool separateGlobalTracks = 0;
#define TRACK_TYPE_ID_LIMIT 100
#define SEPERATE_GLOBAL_TRACKS_MAXID (separateGlobalTracks ? 5 : TRACK_TYPE_ID_LIMIT)
#define SEPERATE_GLOBAL_TRACKS_DISTINGUISH_TYPES 6
bool reorderFinalTracks = 0;

float rotateX = 0, rotateY = 0;
float mouseDnX, mouseDnY;
float mouseMvX, mouseMvY;
bool mouseDn = false;
bool mouseDnR = false;
int mouseWheel = 0;
bool keys[256] = {false}; // Array Used For The Keyboard Routine
bool keysShift[256] = {false}; //Shift held when key down

volatile int exitButton = 0;
volatile int sendKey = 0;

GLfloat currentMatrix[16];
float xyz[3];
float angle[3];
float rphitheta[3];
float quat[4];
template <typename... Args> void SetInfo(Args... args);
void calcXYZ()
{
	xyz[0] = -(currentMatrix[0] * currentMatrix[12] + currentMatrix[1] * currentMatrix[13] + currentMatrix[2] * currentMatrix[14]);
	xyz[1] = -(currentMatrix[4] * currentMatrix[12] + currentMatrix[5] * currentMatrix[13] + currentMatrix[6] * currentMatrix[14]);
	xyz[2] = -(currentMatrix[8] * currentMatrix[12] + currentMatrix[9] * currentMatrix[13] + currentMatrix[10] * currentMatrix[14]);
	
	angle[0] = -asin(currentMatrix[6]); //Invert rotY*rotX*rotZ
	float A = cos(angle[0]);
	if (fabs(A) > 0.005)
	{
		angle[1] = atan2(-currentMatrix[2] / A, currentMatrix[10] / A);
		angle[2] = atan2(currentMatrix[4] / A, currentMatrix[5] / A);
	}
	else
	{
		angle[1] = 0;
		angle[2] = atan2(-currentMatrix[1], -currentMatrix[0]);
	}
	
	rphitheta[0] = sqrt(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);
	rphitheta[1] = atan2(xyz[0], xyz[2]);
	rphitheta[2] = atan2(xyz[1], sqrt(xyz[0] * xyz[0] + xyz[2] * xyz[2]));
	
	createQuaternionFromMatrix(quat, currentMatrix);

	
	/*float angle[1] = -asin(currentMatrix[2]); //Calculate Y-axis angle - for rotX*rotY*rotZ
	float C = cos( angle_y );
	if (fabs(C) > 0.005) //Gimball lock?
	{
		angle[0]  = atan2(-currentMatrix[6] / C, currentMatrix[10] / C);
		angle[2]  = atan2(-currentMatrix[1] / C, currentMatrix[0] / C);
	}
	else
	{
		angle[0]  = 0; //set x-angle
		angle[2]  = atan2(currentMatrix[4], currentMatrix[5]);
	}*/
}

int drawClusters = true;
int drawLinks = false;
int drawSeeds = false;
int drawInitLinks = false;
int drawTracklets = false;
int drawTracks = false;
int drawGlobalTracks = false;
int drawFinal = false;

int drawSlice = -1;
int drawRelatedSlices = 0;
int drawGrid = 0;
int excludeClusters = 0;
int projectxy = 0;

int markClusters = 0;
int hideRejectedClusters = 1;
int hideUnmatchedClusters = 0;
int hideRejectedTracks = 1;

int propagateTracks = 0;
int colorClusters = 1;
int colorCollisions = 0;
std::vector<std::array<int,37>> collisionClusters;
int nCollisions = 1;
int showCollision = -1;
void SetCollisionFirstCluster(unsigned int collision, int slice, int cluster)
{
	nCollisions = collision + 1;
	collisionClusters.resize(nCollisions);
	collisionClusters[collision][slice] = cluster;
}

float Xadd = 0;
float Zadd = 0;

float4 *globalPos = NULL;
int maxClusters = 0;
int currentClusters = 0;

volatile int displayEventNr = 0;
int currentEventNr = -1;

int glDLrecent = 0;
int updateDLList = 0;

float pointSize = 2.0;
float lineWidth = 1.4;

int animate = 0;
HighResTimer animationTimer;
int animationMode = 0;
int animationFrame;
std::vector<float> animateVectors[8];
opengl_spline animationSplines[7];
void animationCloseAngle(float& newangle, float lastAngle)
{
	const float delta = lastAngle > newangle ? (2 * M_PI) : (-2 * M_PI);
	while (fabs(newangle + delta - lastAngle) < fabs(newangle - lastAngle)) newangle += delta;	
}
void animateCloseQuaternion(float* v, float lastx, float lasty, float lastz, float lastw)
{
	float distPos2 = (lastx - v[0]) * (lastx - v[0]) + (lasty - v[1]) * (lasty - v[1]) + (lastz - v[2]) * (lastz - v[2]) + (lastw - v[3]) * (lastw - v[3]);
	float distNeg2 = (lastx + v[0]) * (lastx + v[0]) + (lasty + v[1]) * (lasty + v[1]) + (lastz + v[2]) * (lastz + v[2]) + (lastw + v[3]) * (lastw + v[3]);
	if (distPos2 > distNeg2)
	{
		for (int i = 0;i < 4;i++) v[i] = -v[i];
	}
}
void setAnimationPoint()
{
	float t = animateVectors[0].size();
	if (animationMode & 2) //Spherical
	{
		float r = sqrt(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);
		float r2 = sqrt(xyz[0] * xyz[0] + xyz[2] * xyz[2]);
		float anglePhi = atan2(xyz[0], xyz[2]);
		float angleTheta = atan2(xyz[1], r2);
		if (animateVectors[0].size()) animationCloseAngle(anglePhi, animateVectors[2].back());
		if (animateVectors[0].size()) animationCloseAngle(angleTheta, animateVectors[3].back());
		animateVectors[1].emplace_back(r);
		animateVectors[2].emplace_back(anglePhi);
		animateVectors[3].emplace_back(angleTheta);
	}
	else
	{
		for (int i = 0;i < 3;i++) {animateVectors[i + 1].emplace_back(xyz[i]);} //Cartesian
	}
	if (animationMode & 1) //Euler-angles
	{
		for (int i = 0;i < 3;i++)
		{
			float newangle = angle[i];
			if (animateVectors[0].size()) animationCloseAngle(newangle, animateVectors[i + 4].back());
			animateVectors[i + 4].emplace_back(newangle);
		}
		animateVectors[7].emplace_back(0);
	}
	else //Quaternions
	{
		float v[4];
		createQuaternionFromMatrix(v, currentMatrix);
		if (animateVectors[0].size()) animateCloseQuaternion(v, animateVectors[4].back(), animateVectors[5].back(), animateVectors[6].back(), animateVectors[7].back());
		for (int i = 0;i < 4;i++) animateVectors[i + 4].emplace_back(v[i]);
	}
	animateVectors[0].emplace_back(t);
}
void resetAnimation() {for (int i = 0;i < 8;i++) animateVectors[i].clear();}
void startAnimation()
{
	for (int i = 0;i < 7;i++) animationSplines[i].create(animateVectors[0], animateVectors[i + 1]);
	animationTimer.ResetStart();
	animationFrame = 0;
	animate = 1;
}

volatile int resetScene = 0;

int printInfoText = 1;
char infoText2[1024];
HighResTimer infoText2Timer, infoHelpTimer;
void showInfo(const char* info);
template <typename... Args> void SetInfo(Args... args)
{
	sprintf(infoText2, args...);
	infoText2Timer.ResetStart();
}

inline void SetColorClusters() { if (colorCollisions) return; glColor3f(0, 0.7, 1.0); }
inline void SetColorInitLinks() { glColor3f(0.42, 0.4, 0.1); }
inline void SetColorLinks() { glColor3f(0.8, 0.2, 0.2); }
inline void SetColorSeeds() { glColor3f(0.8, 0.1, 0.85); }
inline void SetColorTracklets() { glColor3f(1, 1, 1); }
inline void SetColorTracks()
{
	if (separateGlobalTracks) glColor3f(1., 1., 0.15);
	else glColor3f(0.4, 1, 0);
}
inline void SetColorGlobalTracks()
{
	if (separateGlobalTracks) glColor3f(1., 0.15, 0.15);
	else glColor3f(1.0, 0.4, 0);
}
inline void SetColorFinal() { if (colorCollisions) return; glColor3f(0, 0.7, 0.2); }
inline void SetColorGrid() { glColor3f(0.7, 0.7, 0.0); }
inline void SetColorMarked() { glColor3f(1.0, 0.0, 0.0); }
inline void SetCollisionColor(int col)
{
	int red = (col * 2) % 5;
	int blue =  (2 + col * 3) % 7;
	int green = (4 + col * 5) % 6;
	if (red == 0 && blue == 0 && green == 0) red = 4;
	glColor3f(red / 4., green / 5., blue / 6.);
}	

void ReSizeGLScene(int width, int height) // Resize And Initialize The GL Window
{
	if (height == 0) // Prevent A Divide By Zero By
	{
		height = 1; // Making Height Equal One
	}

	static int init = 1;

	glViewport(0, 0, width, height); // Reset The Current Viewport

	glMatrixMode(GL_PROJECTION); // Select The Projection Matrix
	glLoadIdentity();
	gluPerspective(45.0f, (GLfloat) width / (GLfloat) height, 0.1f, 1000.0f);

	glMatrixMode(GL_MODELVIEW); // Select The Modelview Matrix

	if (init)
	{
		resetScene = 1;
		init = 0;
		glLoadIdentity();
	}
	else
	{
		glLoadMatrixf(currentMatrix);
	}

	glGetFloatv(GL_MODELVIEW_MATRIX, currentMatrix);
	screen_width = width;
	screen_height = height;
}

void setQuality()
{
	//Doesn't seem to make a difference in this applicattion
	CHKERR(glHint(GL_POINT_SMOOTH_HINT, drawQualityPoint == 2 ? GL_NICEST : drawQualityPoint == 1 ? GL_DONT_CARE : GL_FASTEST));
	CHKERR(glHint(GL_LINE_SMOOTH_HINT, drawQualityLine == 2 ? GL_NICEST : drawQualityLine == 1 ? GL_DONT_CARE : GL_FASTEST));
	CHKERR(glHint(GL_PERSPECTIVE_CORRECTION_HINT, drawQualityPerspective == 2 ? GL_NICEST : drawQualityPerspective == 1 ? GL_DONT_CARE : GL_FASTEST));
	if (drawQualityMSAA)
	{
		CHKERR(glEnable(GL_MULTISAMPLE))
	}
	else
	{
		CHKERR(glDisable(GL_MULTISAMPLE))
	}
}

void setDepthBuffer()
{
	if (depthBuffer)
	{
		glClearDepth(1.0f);                                        // Depth Buffer Setup
		CHKERR(glEnable(GL_DEPTH_TEST));                           // Enables Depth Testing
		CHKERR(glDepthFunc(GL_LEQUAL));                            // The Type Of Depth Testing To Do
	}
	else
	{
		CHKERR(glDisable(GL_DEPTH_TEST));
	}
}

int InitGL()
{
	CHKERR(glewInit());
	CHKERR(glGenBuffers(1, &vbo_id));
	CHKERR(glBindBuffer(GL_ARRAY_BUFFER, vbo_id));
	CHKERR(glGenBuffers(1, &indirect_id));
	CHKERR(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_id));
	CHKERR(glShadeModel(GL_SMOOTH));                           // Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);                      // Black Background
	setDepthBuffer();
	setQuality();
	ReSizeGLScene(init_width, init_height);
	return (true);                                     // Initialization Went OK
}

inline void drawPointLinestrip(int iSlice, int cid, int id, int id_limit = TRACK_TYPE_ID_LIMIT)
{
	vertexBuffer[iSlice].emplace_back(globalPos[cid].x, globalPos[cid].y, projectxy ? 0 : globalPos[cid].z);
	if (globalPos[cid].w < id_limit) globalPos[cid].w = id;
}

vboList DrawClusters(AliHLTTPCCATracker &tracker, int select, unsigned int iCol)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	size_t startCountInner = vertexBuffer[iSlice].size();
	for (int i = 0; i < tracker.Param().NRows(); i++)
	{
		const AliHLTTPCCARow &row = tracker.Data().Row(i);
		for (int j = 0; j < row.NHits(); j++)
		{
			const int cidInSlice = tracker.Data().ClusterDataIndex(row, j);
			const int cid = tracker.ClusterData()->Id(cidInSlice);
			if (hideUnmatchedClusters && SuppressHit(cid)) continue;
			if (nCollisions > 1)
			{
				unsigned int k = 0;
				while (k < collisionClusters.size() && collisionClusters[k][tracker.Param().ISlice()] < cidInSlice) k++;
				if (k != iCol) continue;
			}
			bool draw = globalPos[cid].w == select;
			if (markClusters)
			{
				const short flags = tracker.ClusterData()->Flags(tracker.Data().ClusterDataIndex(row, j));
				const bool match = flags & markClusters;
				draw = (select == 8) ? (match) : (draw && !match);
			}
			if (draw)
			{
				vertexBuffer[iSlice].emplace_back(globalPos[cid].x, globalPos[cid].y, projectxy ? 0 : globalPos[cid].z);
			}
		}
	}
	insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawLinks(AliHLTTPCCATracker &tracker, int id, bool dodown = false)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	size_t startCountInner = vertexBuffer[iSlice].size();
	for (int i = 0; i < tracker.Param().NRows(); i++)
	{
		const AliHLTTPCCARow &row = tracker.Data().Row(i);

		if (i < tracker.Param().NRows() - 2)
		{
			const AliHLTTPCCARow &rowUp = tracker.Data().Row(i + 2);
			for (int j = 0; j < row.NHits(); j++)
			{
				if (tracker.Data().HitLinkUpData(row, j) != CALINK_INVAL)
				{
					const int cid1 = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(row, j));
					const int cid2 = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(rowUp, tracker.Data().HitLinkUpData(row, j)));
					drawPointLinestrip(iSlice, cid1, id);
					drawPointLinestrip(iSlice, cid2, id);
				}
			}
		}

		if (dodown && i >= 2)
		{
			const AliHLTTPCCARow &rowDown = tracker.Data().Row(i - 2);
			for (int j = 0; j < row.NHits(); j++)
			{
				if (tracker.Data().HitLinkDownData(row, j) != CALINK_INVAL)
				{
					const int cid1 = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(row, j));
					const int cid2 = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(rowDown, tracker.Data().HitLinkDownData(row, j)));
					drawPointLinestrip(iSlice, cid1, id);
					drawPointLinestrip(iSlice, cid2, id);
				}
				}
		}
	}
	insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawSeeds(AliHLTTPCCATracker &tracker)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	for (int i = 0; i < *tracker.NTracklets(); i++)
	{
		const AliHLTTPCCAHitId &hit = tracker.TrackletStartHit(i);
		size_t startCountInner = vertexBuffer[iSlice].size();
		int ir = hit.RowIndex();
		calink ih = hit.HitIndex();
		do
		{
			const AliHLTTPCCARow &row = tracker.Data().Row(ir);
			const int cid = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(row, ih));
			drawPointLinestrip(iSlice, cid, 3);
			ir += 2;
			ih = tracker.Data().HitLinkUpData(row, ih);
		} while (ih != CALINK_INVAL);
		insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	}
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawTracklets(AliHLTTPCCATracker &tracker)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	for (int i = 0; i < *tracker.NTracklets(); i++)
	{
		const AliHLTTPCCATracklet &tracklet = tracker.Tracklet(i);
		if (tracklet.NHits() == 0) continue;
		size_t startCountInner = vertexBuffer[iSlice].size();
		float4 oldpos;
		for (int j = tracklet.FirstRow(); j <= tracklet.LastRow(); j++)
		{
#ifdef EXTERN_ROW_HITS
			const calink rowHit = tracker.TrackletRowHits()[j * *tracker.NTracklets() + i];
#else
			const calink rowHit = tracklet.RowHit(j);
#endif
			if (rowHit != CALINK_INVAL)
			{
				const AliHLTTPCCARow &row = tracker.Data().Row(j);
				const int cid = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(row, rowHit));
				/*if (j != tracklet.FirstRow())
				{
					float dist = (oldpos.x - globalPos[cid].x) * (oldpos.x - globalPos[cid].x) + (oldpos.y - globalPos[cid].y) * (oldpos.y - globalPos[cid].y) + (oldpos.z - globalPos[cid].z) * (oldpos.z - globalPos[cid].z);
				}*/
				oldpos = globalPos[cid];
				drawPointLinestrip(iSlice, cid, 4);
			}
		}
		insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	}
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawTracks(AliHLTTPCCATracker &tracker, int global)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	for (int i = (global ? tracker.CommonMemory()->fNLocalTracks : 0); i < (global ? *tracker.NTracks() : tracker.CommonMemory()->fNLocalTracks); i++)
	{
		AliHLTTPCCATrack &track = tracker.Tracks()[i];
		size_t startCountInner = vertexBuffer[iSlice].size();
		for (int j = 0; j < track.NHits(); j++)
		{
			const AliHLTTPCCAHitId &hit = tracker.TrackHits()[track.FirstHitID() + j];
			const AliHLTTPCCARow &row = tracker.Data().Row(hit.RowIndex());
			const int cid = tracker.ClusterData()->Id(tracker.Data().ClusterDataIndex(row, hit.HitIndex()));
			drawPointLinestrip(iSlice, cid, 5 + global);
		}
		insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	}
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawFinal(AliHLTTPCCAStandaloneFramework &hlt, int iSlice, unsigned int iCol, AliHLTTPCGMPropagator* prop)
{
	size_t startCount = vertexBufferStart[iSlice].size();
	std::vector<GLvertex> buffer;

	const AliHLTTPCGMMerger &merger = hlt.Merger();
	int trackCount = propagateTracks == 3 ? hlt.GetNMCInfo() : merger.NOutputTracks();
	for (int i = 0; i < trackCount; i++)
	{
		const AliHLTTPCGMMergedTrack &track = merger.OutputTracks()[i];
		int *clusterused = NULL;
		int bestk = 0;
		if (propagateTracks < 3)
		{
			if (track.NClusters() == 0) continue;
			if (hideRejectedTracks && !track.OK()) continue;
			if (merger.Clusters()[track.FirstClusterRef() + track.NClusters() - 1].fSlice != iSlice) continue;
		}
		if (nCollisions > 1)
		{
			int label = propagateTracks == 3 ? i : GetMCLabel(i);
			if (label < -1) label = -label - 2;
			if (label != -1)
			{
				unsigned int k = 0;
				while (k < collisionClusters.size() && collisionClusters[k][36] < label) k++;
				if (k != iCol) continue;
			}
		}

		size_t startCountInner = vertexBuffer[iSlice].size();
		if (propagateTracks <= 1)
		{
			if (reorderFinalTracks)
			{
				clusterused = new int[track.NClusters()];
				for (int j = 0; j < track.NClusters(); j++)
					clusterused[j] = 0;

				float smallest = 1e20;
				for (int k = 0; k < track.NClusters(); k++)
				{
					if (hideRejectedClusters && (merger.Clusters()[track.FirstClusterRef() + k].fState & AliHLTTPCGMMergedTrackHit::flagReject)) continue;
					int cid = merger.Clusters()[track.FirstClusterRef() + k].fId;
					float dist = globalPos[cid].x * globalPos[cid].x + globalPos[cid].y * globalPos[cid].y + globalPos[cid].z * globalPos[cid].z;
					if (dist < smallest)
					{
						smallest = dist;
						bestk = k;
					}
				}
			}
			else
			{
				while (hideRejectedClusters && (merger.Clusters()[track.FirstClusterRef() + bestk].fState & AliHLTTPCGMMergedTrackHit::flagReject)) bestk++;
			}

			int lastcid = merger.Clusters()[track.FirstClusterRef() + bestk].fId;
			if (reorderFinalTracks) clusterused[bestk] = 1;

			bool linestarted = (globalPos[lastcid].w < SEPERATE_GLOBAL_TRACKS_DISTINGUISH_TYPES);
			if (!separateGlobalTracks || linestarted)
			{
				drawPointLinestrip(iSlice, lastcid, 7, SEPERATE_GLOBAL_TRACKS_MAXID);
			}

			for (int j = (reorderFinalTracks ? 1 : (bestk + 1)); j < track.NClusters(); j++)
			{
				int bestcid = 0;
				if (reorderFinalTracks)
				{
					bestk = 0;
					float bestdist = 1e20;
					for (int k = 0; k < track.NClusters(); k++)
					{
						if (clusterused[k]) continue;
						if (hideRejectedClusters && (merger.Clusters()[track.FirstClusterRef() + k].fState & AliHLTTPCGMMergedTrackHit::flagReject)) continue;
						int cid = merger.Clusters()[track.FirstClusterRef() + k].fId;
						float dist = (globalPos[cid].x - globalPos[lastcid].x) * (globalPos[cid].x - globalPos[lastcid].x) +
						             (globalPos[cid].y - globalPos[lastcid].y) * (globalPos[cid].y - globalPos[lastcid].y) +
						             (globalPos[cid].z - globalPos[lastcid].z) * (globalPos[cid].z - globalPos[lastcid].z);
						if (dist < bestdist)
						{
							bestdist = dist;
							bestcid = cid;
							bestk = k;
						}
					}
					if (bestdist > 1e19) continue;
				}
				else
				{
					if (hideRejectedClusters && (merger.Clusters()[track.FirstClusterRef() + j].fState & AliHLTTPCGMMergedTrackHit::flagReject)) continue;
					bestcid = merger.Clusters()[track.FirstClusterRef() + j].fId;
				}
				if (separateGlobalTracks && !linestarted && globalPos[bestcid].w < SEPERATE_GLOBAL_TRACKS_DISTINGUISH_TYPES)
				{
					drawPointLinestrip(iSlice, lastcid, 7, SEPERATE_GLOBAL_TRACKS_MAXID);
					linestarted = true;
				}
				if (!separateGlobalTracks || linestarted)
				{
					drawPointLinestrip(iSlice, bestcid, 7, SEPERATE_GLOBAL_TRACKS_MAXID);
				}
				if (separateGlobalTracks && linestarted && !(globalPos[bestcid].w < SEPERATE_GLOBAL_TRACKS_DISTINGUISH_TYPES)) linestarted = false;
				if (reorderFinalTracks) clusterused[bestk] = 1;
				lastcid = bestcid;
			}
		}
		else if (propagateTracks == 2)
		{
			for (int k = 0; k < track.NClusters(); k++)
			{
				if (hideRejectedClusters && (merger.Clusters()[track.FirstClusterRef() + k].fState & AliHLTTPCGMMergedTrackHit::flagReject)) continue;
				int cid = merger.Clusters()[track.FirstClusterRef() + k].fId;
				if (globalPos[cid].w < SEPERATE_GLOBAL_TRACKS_MAXID) globalPos[cid].w = 7;
			}
		}
		if (propagateTracks)
		{
			int flyRounds = (propagateTracks >= 2 ? 2 : 1);
			for (int inFlyDirection = 0;inFlyDirection < flyRounds;inFlyDirection++)
			{
				AliHLTTPCGMTrackParam param;
				float alpha;
				float x = 0;
				int slice = 0;
				if (propagateTracks < 3)
				{
					param = track.GetParam();
					alpha = track.GetAlpha();
					for (int k = track.NClusters() - 1;k >= 0;k--)
					{
						auto cl = merger.Clusters()[track.FirstClusterRef() + track.NClusters() - 1];
						if (cl.fState & AliHLTTPCGMMergedTrackHit::flagReject) continue;
						slice = cl.fSlice;
						x = cl.fX + (inFlyDirection ? 0 : -1);
					}
				}
				else
				{
					param.ResetCovariance();
					const AliHLTTPCCAMCInfo& mc = hlt.GetMCInfo()[i];
					if (mc.fCharge == 0.f) break;
					if (mc.fPID < 0) break;
					
					alpha = atan2(mc.fY, mc.fX);
					if (alpha < 0) alpha += 2 * M_PI;
					slice = floor(alpha / (2 * M_PI) * 18);
					if (mc.fZ < 0) slice += 18;
					if (slice != iSlice) break;
					alpha = hlt.Param().Alpha(slice);
					float c = cos(alpha);
					float s = sin(alpha);
					
					float mclocal[4];
					x = mc.fX;
					float y = mc.fY;
					mclocal[0] = x*c + y*s;
					mclocal[1] =-x*s + y*c;
					float px = mc.fPx;
					float py = mc.fPy;
					mclocal[2] = px*c + py*s;
					mclocal[3] =-px*s + py*c;
					float pt = sqrt(px * px + py * py);
					if (pt < 0.001) break;
					float sinPhi = mclocal[3] / pt;
					float charge = mc.fCharge > 0 ? 1.f : -1.f;
					float qpt = charge / pt;
					float dzds = mc.fPz / pt;
					
					x = mclocal[0];
					param.X() = mclocal[0];
					param.Y() = mclocal[1];
					param.Z() = mc.fZ;
					param.SinPhi() = sinPhi;
					param.DzDs() = dzds;
					param.QPt() = qpt;
					param.ZOffset() = 0.f;
					
					//printf("Track: XYZ %f %f %f, P %f %f %f, x %f y %f z %f pt %f sinphi %f dzds %f qpt %f\n", mc.fX, mc.fY, mc.fZ, mc.fPx, mc.fPy, mc.fPz, x, param.Y(), param.Z(), pt, param.SinPhi(), param.DzDs(), param.QPt());
				}
				if (flyRounds == 2) buffer.clear();
				prop->SetTrack(&param, alpha);
				if (x < 1) break;
				alpha = hlt.Param().Alpha(slice);
				std::vector<GLvertex>& useBuffer = flyRounds == 2 && inFlyDirection == 0 ? buffer : vertexBuffer[iSlice];
				
				while (x > 1. && x <= 250 && fabs(param.Z() + param.ZOffset()) <= maxClusterZ)
				{
					if (prop->PropagateToXAlpha( x, alpha, inFlyDirection ) ) break;
					if (fabs(param.SinPhi()) > 0.9) break;
					float4 ptr;
					hlt.Tracker().CPUTracker(slice).Param().Slice2Global(param.X() + Xadd, param.Y(), param.Z(), &ptr.x, &ptr.y, &ptr.z);
					useBuffer.emplace_back(ptr.x / GL_SCALE_FACTOR, ptr.y / GL_SCALE_FACTOR, projectxy ? 0 : (ptr.z + param.ZOffset()) / GL_SCALE_FACTOR);
					x += inFlyDirection ? 1 : -1;
				}
				
				if (flyRounds == 2 && inFlyDirection == 0)
				{
					for (int k = (int) buffer.size() - 1;k >= 0;k--)
					{
						vertexBuffer[iSlice].emplace_back(buffer[k]);
					}
				}
			}
		}
		insertVertexList(iSlice, startCountInner, vertexBuffer[iSlice].size());
		if (reorderFinalTracks) delete[] clusterused;
	}
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

vboList DrawGrid(AliHLTTPCCATracker &tracker)
{
	int iSlice = tracker.Param().ISlice();
	size_t startCount = vertexBufferStart[iSlice].size();
	size_t startCountInner = vertexBuffer[iSlice].size();
	for (int i = 0; i < tracker.Param().NRows(); i++)
	{
		const AliHLTTPCCARow &row = tracker.Data().Row(i);
		for (int j = 0; j <= (signed) row.Grid().Ny(); j++)
		{
			float z1 = row.Grid().ZMin();
			float z2 = row.Grid().ZMax();
			float x = row.X() + Xadd;
			float y = row.Grid().YMin() + (float) j / row.Grid().StepYInv();
			float zz1, zz2, yy1, yy2, xx1, xx2;
			tracker.Param().Slice2Global(x, y, z1, &xx1, &yy1, &zz1);
			tracker.Param().Slice2Global(x, y, z2, &xx2, &yy2, &zz2);
			if (zz1 >= 0)
			{
				zz1 += Zadd;
				zz2 += Zadd;
			}
			else
			{
				zz1 -= Zadd;
				zz2 -= Zadd;
			}
			vertexBuffer[iSlice].emplace_back(xx1 / GL_SCALE_FACTOR, yy1 / GL_SCALE_FACTOR, zz1 / GL_SCALE_FACTOR);
			vertexBuffer[iSlice].emplace_back(xx2 / GL_SCALE_FACTOR, yy2 / GL_SCALE_FACTOR, zz2 / GL_SCALE_FACTOR);
		}
		for (int j = 0; j <= (signed) row.Grid().Nz(); j++)
		{
			float y1 = row.Grid().YMin();
			float y2 = row.Grid().YMax();
			float x = row.X() + Xadd;
			float z = row.Grid().ZMin() + (float) j / row.Grid().StepZInv();
			float zz1, zz2, yy1, yy2, xx1, xx2;
			tracker.Param().Slice2Global(x, y1, z, &xx1, &yy1, &zz1);
			tracker.Param().Slice2Global(x, y2, z, &xx2, &yy2, &zz2);
			if (zz1 >= 0)
			{
				zz1 += Zadd;
				zz2 += Zadd;
			}
			else
			{
				zz1 -= Zadd;
				zz2 -= Zadd;
			}
			vertexBuffer[iSlice].emplace_back(xx1 / GL_SCALE_FACTOR, yy1 / GL_SCALE_FACTOR, zz1 / GL_SCALE_FACTOR);
			vertexBuffer[iSlice].emplace_back(xx2 / GL_SCALE_FACTOR, yy2 / GL_SCALE_FACTOR, zz2 / GL_SCALE_FACTOR);
		}
	}
	insertVertexList(tracker.Param().ISlice(), startCountInner, vertexBuffer[iSlice].size());
	return vboList(startCount, vertexBufferStart[iSlice].size() - startCount, iSlice);
}

int DrawGLScene(bool doAnimation) // Here's Where We Do All The Drawing
{
	static float fpsscale = 1, fpsscaleadjust = 0;

	static int framesDone = 0, framesDoneFPS = 0;
	static HighResTimer timerFPS, timerDisplay, timerDraw;
	bool showTimer = false;

	constexpr const int N_POINTS_TYPE = 9;
	constexpr const int N_LINES_TYPE = 6;
	static vboList glDLlines[fgkNSlices][N_LINES_TYPE];
	static std::vector<vboList> glDLfinal[fgkNSlices];
	static std::vector<vboList> GLpoints[fgkNSlices][N_POINTS_TYPE];
	static vboList glDLgrid[fgkNSlices];

	AliHLTTPCCAStandaloneFramework &hlt = AliHLTTPCCAStandaloneFramework::Instance();
	
	//Initialize
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear Screen And Depth Buffer
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();                                   // Reset The Current Modelview Matrix

	int mouseWheelTmp = mouseWheel;
	mouseWheel = 0;
	bool lookOrigin = camLookOrigin ^ keys[KEY_ALT];
	bool yUp = camYUp ^ keys[KEY_CTRL] ^ lookOrigin;

	//Calculate rotation / translation scaling factors
	float scalefactor = keys[KEY_SHIFT] ? 0.2 : 1.0;
	float rotatescalefactor = scalefactor * 0.25f;
	if (drawSlice != -1)
	{
		scalefactor *= 0.2f;
	}
	float sqrdist = sqrt(sqrt(currentMatrix[12] * currentMatrix[12] + currentMatrix[13] * currentMatrix[13] + currentMatrix[14] * currentMatrix[14]) / GL_SCALE_FACTOR) * 0.8;
	if (sqrdist < 0.2) sqrdist = 0.2;
	if (sqrdist > 5) sqrdist = 5;
	scalefactor *= sqrdist;

	//Perform new rotation / translation
	if (animate)
	{
		float time = animationTimer.GetCurrentElapsedTime() / 3.f;
		//float time = animationFrame / 30.f;
		float maxTime = animateVectors[0].back();
		animationFrame++;
		if (time >= maxTime)
		{
			time = maxTime;
			animate = 0;
			SetInfo("Animation finished. (%f seconds, %d frames)", time, animationFrame);
		}
		else
		{
			SetInfo("Running animation: time %f/%f, frames %d", time, maxTime, animationFrame);
		}
		float vals[7];
		for (int i = 0;i < 7;i++)
		{
			vals[i] = animationSplines[i].evaluate(time);
		}
		if (animationMode & 1) //Rotation from euler angles
		{
			glRotatef(-vals[3] * 180.f / M_PI, 1, 0, 0);
			glRotatef(vals[4] * 180.f / M_PI, 0, 1, 0);
			glRotatef(-vals[5] * 180.f / M_PI, 0, 0, 1);
		}
		else //Rotation from quaternion
		{
			float mag = sqrt(vals[3] * vals[3] + vals[4] * vals[4] + vals[5] * vals[5] + vals[6] * vals[6]);
			if (mag < 0.0001) vals[6] = 1;
			else for (int i = 0;i < 4;i++) vals[3 + i] /= mag;

			float xx = vals[3] * vals[3], xy = vals[3] * vals[4], xz = vals[3] * vals[5], xw = vals[3] * vals[6], yy = vals[4] * vals[4], yz = vals[4] * vals[5], yw = vals[4] * vals[6], zz = vals[5] * vals[5], zw = vals[5] * vals[6];
			float mat[16] = {1 - 2 * (yy + zz), 2 * (xy - zw), 2 * (xz + yw), 0, 2 * (xy + zw),  1 - 2 * (xx + zz), 2 * (yz - xw), 0, 2 * (xz - yw), 2 * (yz + xw), 1 - 2 * (xx + yy), 0, 0, 0, 0, 1};
			glMultMatrixf(mat);
		}
		if (animationMode & 2) //Compute cartesian translation from sperical coordinates
		{
			float r = vals[0], phi = vals[1], theta = vals[2];
			vals[2] = r * cos(phi) * cos(theta);
			vals[0] = r * sin(phi) * cos(theta);
			vals[1] = r * sin(theta);
		}
		glTranslatef(-vals[0], -vals[1], -vals[2]);
		
		/*static int nFrame = 0;

		DrawGLScene(true);
		char filename[16];
		sprintf(filename, "video%05d.bmp", nFrame++);
		unsigned char *mixBuffer = NULL;
		//...
		DoScreenshot(NULL, 1, &mixBuffer);

		//...
		DoScreenshot(filename, 1, &mixBuffer, (float) (nFrame % 30) / 30.f);
		free(mixBuffer);
		printf("Wrote video frame %s\n\n", filename);*/
		
	}
	else if (resetScene)
	{
		glTranslatef(0, 0, -8);

		pointSize = 2.0;
		drawSlice = -1;
		Xadd = Zadd = 0;
		camLookOrigin = camYUp = false;
		angleRollOrigin = -1e9;

		resetScene = 0;
		updateDLList = true;
	}
	else
	{
		float moveZ = scalefactor * ((float) mouseWheelTmp / 150 + (float) (keys['W'] - keys['S']) * (!keys[KEY_SHIFT]) * 0.2 * fpsscale);
		float moveY = scalefactor * ((float) (keys[KEY_PAGEDOWN] - keys[KEY_PAGEUP]) * 0.2 * fpsscale);
		float moveX = scalefactor * ((float) (keys['A'] - keys['D']) * (!keys[KEY_SHIFT]) * 0.2 * fpsscale);
		float rotRoll = rotatescalefactor * fpsscale * 2 * (keys['E'] - keys['F']) * (!keys[KEY_SHIFT]);
		float rotYaw = rotatescalefactor * fpsscale * 2 * (keys[KEY_RIGHT] - keys[KEY_LEFT]);
		float rotPitch = rotatescalefactor * fpsscale * 2 * (keys[KEY_DOWN] - keys[KEY_UP]);

		if (mouseDnR && mouseDn)
		{
			moveZ += -scalefactor * ((float) mouseMvY - (float) mouseDnY) / 4;
			rotRoll += rotatescalefactor * ((float) mouseMvX - (float) mouseDnX);
		}
		else if (mouseDnR)
		{
			moveX += -scalefactor * 0.5 * ((float) mouseDnX - (float) mouseMvX) / 4;
			moveY += -scalefactor * 0.5 * ((float) mouseMvY - (float) mouseDnY) / 4;
		}
		else if (mouseDn)
		{
			rotYaw += rotatescalefactor * ((float) mouseMvX - (float) mouseDnX);
			rotPitch += rotatescalefactor * ((float) mouseMvY - (float) mouseDnY);
		}

		if (yUp) angleRollOrigin = 0;
		else if (!lookOrigin) angleRollOrigin = -1e6;
		if (lookOrigin)
		{
			if (!yUp)
			{
				if (angleRollOrigin < -1e6) angleRollOrigin = yUp ? 0. : -angle[2];
				angleRollOrigin += rotRoll;
				glRotatef(angleRollOrigin, 0, 0, 1);
				float tmpX = moveX, tmpY = moveY;
				moveX = tmpX * cos(angle[2]) - tmpY * sin(angle[2]);
				moveY = tmpX * sin(angle[2]) + tmpY * cos(angle[2]);
			}
			
			const float x = xyz[0], y = xyz[1], z = xyz[2];
			float r = sqrt(x * x + + y * y + z * z);
			float r2 = sqrt(x * x + z * z);
			float phi = atan2(z, x);
			phi += moveX * 0.1f;
			float theta = atan2(xyz[1], r2);
			theta -= moveY * 0.1f;
			const float max_theta = M_PI / 2 - 0.01;
			if (theta >= max_theta) theta = max_theta;
			else if (theta <= -max_theta) theta = -max_theta;
			if (moveZ >= r - 0.1) moveZ = r - 0.1;
			r -= moveZ;
			r2 = r * cos(theta);
			xyz[0] = r2 * cos(phi);
			xyz[2] = r2 * sin(phi);
			xyz[1] = r * sin(theta);
			
			gluLookAt(xyz[0], xyz[1], xyz[2], 0, 0, 0, 0, 1, 0);
		}
		else
		{
			glTranslatef(moveX, moveY, moveZ);
			if (rotYaw != 0.f) glRotatef(rotYaw, 0, 1, 0);
			if (rotPitch != 0.f) glRotatef(rotPitch, 1, 0, 0);
			if (!yUp && rotRoll != 0.f) glRotatef(rotRoll, 0, 0, 1);
			glMultMatrixf(currentMatrix); //Apply previous translation / rotation
			
			if (yUp)
			{
				glGetFloatv(GL_MODELVIEW_MATRIX, currentMatrix);
				calcXYZ();
				glLoadIdentity();
				glRotatef(angle[2] * 180.f / M_PI, 0, 0, 1);
				glMultMatrixf(currentMatrix);
			}
		}

		//Graphichs Options
		int deltaLine = keys['+']*keysShift['+'] - keys['-']*keysShift['-'];
		lineWidth += (float) deltaLine * fpsscale * 0.05;
		if (lineWidth < 0.01) lineWidth = 0.01;
		if (deltaLine) SetInfo("%s line width: %f", deltaLine > 0 ? "Increasing" : "Decreasing", lineWidth); 
		int deltaPoint = keys['+']*(!keysShift['+']) - keys['-']*(!keysShift['-']);
		pointSize += (float) deltaPoint * fpsscale * 0.05;
		if (pointSize < 0.01) pointSize = 0.01;
		if (deltaPoint) SetInfo("%s point size: %f", deltaPoint > 0 ? "Increasing" : "Decreasing", pointSize);
	}
	
	//Store position
	glGetFloatv(GL_MODELVIEW_MATRIX, currentMatrix);
	calcXYZ();
	
	if (mouseDn || mouseDnR)
	{
		mouseDnX = mouseMvX;
		mouseDnY = mouseMvY;
	}	

//Make sure event gets not overwritten during display
#ifdef R__WIN32
	WaitForSingleObject(semLockDisplay, INFINITE);
#else
	pthread_mutex_lock(&semLockDisplay);
#endif

	//Open GL Default Values
	if (smoothPoints)
	{
		CHKERR(glEnable(GL_POINT_SMOOTH));
	}
	else
	{
		CHKERR(glDisable(GL_POINT_SMOOTH));
	}
	if (smoothLines)
	{
		CHKERR(glEnable(GL_LINE_SMOOTH));
	}
	else
	{
		CHKERR(glDisable(GL_LINE_SMOOTH));
	}
	CHKERR(glEnable(GL_BLEND));
	CHKERR(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	CHKERR(glPointSize(pointSize));
	CHKERR(glLineWidth(lineWidth));

	//Extract global cluster information
	if (updateDLList || displayEventNr != currentEventNr)
	{
		showTimer = true;
		timerDraw.ResetStart();
		currentClusters = 0;
		for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
		{
			currentClusters += hlt.Tracker().CPUTracker(iSlice).NHitsTotal();
		}

		if (maxClusters < currentClusters)
		{
			if (globalPos) delete[] globalPos;
			maxClusters = currentClusters;
			globalPos = new float4[maxClusters];
		}

		maxClusterZ = 0;
		for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
		{
			const AliHLTTPCCAClusterData &cdata = hlt.ClusterData(iSlice);
			for (int i = 0; i < cdata.NumberOfClusters(); i++)
			{
				const int cid = cdata.Id(i);
				if (cid >= maxClusters)
				{
					printf("Cluster Buffer Size exceeded (id %d max %d)\n", cid, maxClusters);
					exit(1);
				}
				float4 *ptr = &globalPos[cid];
				hlt.Tracker().CPUTracker(iSlice).Param().Slice2Global(cdata.X(i) + Xadd, cdata.Y(i), cdata.Z(i), &ptr->x, &ptr->y, &ptr->z);
				if (fabs(ptr->z) > maxClusterZ) maxClusterZ = fabs(ptr->z);
				if (ptr->z >= 0)
				{
					ptr->z += Zadd;
					ptr->z += Zadd;
				}
				else
				{
					ptr->z -= Zadd;
					ptr->z -= Zadd;
				}

				ptr->x /= GL_SCALE_FACTOR;
				ptr->y /= GL_SCALE_FACTOR;
				ptr->z /= GL_SCALE_FACTOR;
				ptr->w = 1;
			}
		}

		currentEventNr = displayEventNr;

		timerFPS.ResetStart();
		framesDoneFPS = 0;
		fpsscaleadjust = 0;
		glDLrecent = 0;
		updateDLList = 0;
	}

	//Prepare Event
	if (!glDLrecent)
	{
		for (int i = 0;i < fgkNSlices;i++)
		{
			vertexBuffer[i].clear();
			vertexBufferStart[i].clear();
			vertexBufferCount[i].clear();
		}

		for (int i = 0; i < currentClusters; i++) globalPos[i].w = 0;

		for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
		{
			for (int i = 0;i < N_POINTS_TYPE;i++) GLpoints[iSlice][i].resize(nCollisions);
			glDLfinal[iSlice].resize(nCollisions);
		}
#pragma omp parallel
		{
#pragma omp for
			for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
			{
				AliHLTTPCCATracker &tracker = hlt.Tracker().CPUTracker(iSlice);
				if (drawInitLinks)
				{
					char *tmpMem;
					if (tracker.fLinkTmpMemory == NULL)
					{
						printf("Need to set TRACKER_KEEP_TEMPDATA for visualizing PreLinks!\n");
						continue;
					}
					tmpMem = tracker.Data().Memory();
					tracker.SetGPUSliceDataMemory((void *) tracker.fLinkTmpMemory, tracker.Data().Rows());
					tracker.SetPointersSliceData(tracker.ClusterData());
					glDLlines[iSlice][0] = DrawLinks(tracker, 1, true);
					tracker.SetGPUSliceDataMemory(tmpMem, tracker.Data().Rows());
					tracker.SetPointersSliceData(tracker.ClusterData());
				}
			}
#pragma omp barrier

			AliHLTTPCGMPropagator prop;
			const float kRho = 1.025e-3;//0.9e-3;
			const float kRadLen = 29.532;//28.94;
			prop.SetMaxSinPhi(.999);
			prop.SetMaterial(kRadLen, kRho);
			prop.SetPolynomialField(hlt.Merger().pField());		
			prop.SetToyMCEventsFlag(hlt.Merger().SliceParam().ToyMCEventsFlag());
#pragma omp for
			for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
			{
				AliHLTTPCCATracker &tracker = hlt.Tracker().CPUTracker(iSlice);

				glDLlines[iSlice][1] = DrawLinks(tracker, 2);
				glDLlines[iSlice][2] = DrawSeeds(tracker);
				glDLlines[iSlice][3] = DrawTracklets(tracker);
				glDLlines[iSlice][4] = DrawTracks(tracker, 0);
				glDLlines[iSlice][5] = DrawTracks(tracker, 1);
				glDLgrid[iSlice] = DrawGrid(tracker);

				for (int iCol = 0;iCol < nCollisions;iCol++)
				{
					glDLfinal[iSlice][iCol] = DrawFinal(hlt, iSlice, iCol, &prop);
				}
			}
#pragma omp barrier
#pragma omp for
			for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
			{
				AliHLTTPCCATracker &tracker = hlt.Tracker().CPUTracker(iSlice);
				for (int i = 0; i < N_POINTS_TYPE; i++)
				{
					for (int iCol = 0;iCol < nCollisions;iCol++)
					{
						GLpoints[iSlice][i][iCol] = DrawClusters(tracker, i, iCol);
					}
				}
			}
		}

		glDLrecent = 1;
		size_t totalVertizes = 0;
		for (int i = 0;i < fgkNSlices;i++) totalVertizes += vertexBuffer[i].size();
		vertexBuffer[0].reserve(totalVertizes); //ATTENTION, this only reserves but not initializes, I.e. we use the vector as raw ptr for now...
		size_t totalYet = vertexBuffer[0].size();
		for (int i = 1;i < fgkNSlices;i++)
		{
			for (unsigned int j = 0;j < vertexBufferStart[i].size();j++)
			{
				vertexBufferStart[i][j] += totalYet;
			}
			memcpy(&vertexBuffer[0][totalYet], &vertexBuffer[i][0], vertexBuffer[i].size() * sizeof(vertexBuffer[i][0]));
			totalYet += vertexBuffer[i].size();
			vertexBuffer[i].clear();
		}
		CHKERR(glBufferData(GL_ARRAY_BUFFER, totalVertizes * sizeof(vertexBuffer[0][0]), vertexBuffer[0].data(), GL_STATIC_DRAW));
		vertexBuffer[0].clear();
		
		if (useGLIndirectDraw)
		{
			std::vector<DrawArraysIndirectCommand> cmds;
			for (int iSlice = 0;iSlice < fgkNSlices;iSlice++)
			{
				indirectSliceOffset[iSlice] = cmds.size();
				for (unsigned int k = 0;k < vertexBufferStart[iSlice].size();k++)
				{
					cmds.emplace_back(vertexBufferCount[iSlice][k], 1, vertexBufferStart[iSlice][k], 0);
				}
			}
			CHKERR(glBufferData(GL_DRAW_INDIRECT_BUFFER, cmds.size() * sizeof(cmds[0]), cmds.data(), GL_STATIC_DRAW));
		}
	}
	if (showTimer)
	{
		printf("Draw time: %d ms\n", (int) (timerDraw.GetCurrentElapsedTime() * 1000000.));
	}

	//Draw Event
	drawCalls = 0;
	CHKERR(glEnableClientState(GL_VERTEX_ARRAY));
	CHKERR(glVertexPointer(3, GL_FLOAT, 0, 0));
	for (int iSlice = 0; iSlice < fgkNSlices; iSlice++)
	{
		if (drawSlice != -1)
		{
			if (!drawRelatedSlices && drawSlice != iSlice) continue;
			if (drawRelatedSlices && (drawSlice % 9) != (iSlice % 9)) continue;
		}

		if (drawGrid)
		{
			SetColorGrid();
			drawVertices(glDLgrid[iSlice], GL_LINES);
		}

		if (drawClusters)
		{
			for (int iCol = 0;iCol < nCollisions;iCol++)
			{
				SetColorClusters();
				if (showCollision != -1) iCol = showCollision;
				if (colorCollisions) SetCollisionColor(iCol);
				drawVertices(GLpoints[iSlice][0][iCol], GL_POINTS);

				if (drawInitLinks)
				{
					if (excludeClusters) goto skip1;
					if (colorClusters) SetColorInitLinks();
				}
				drawVertices(GLpoints[iSlice][1][iCol], GL_POINTS);

				if (drawLinks)
				{
					if (excludeClusters) goto skip1;
					if (colorClusters) SetColorLinks();
				}
				else
				{
					SetColorClusters();
				}
				drawVertices(GLpoints[iSlice][2][iCol], GL_POINTS);

				if (drawSeeds)
				{
					if (excludeClusters) goto skip1;
					if (colorClusters) SetColorSeeds();
				}
				drawVertices(GLpoints[iSlice][3][iCol], GL_POINTS);

			skip1:
				SetColorClusters();
				if (drawTracklets)
				{
					if (excludeClusters) goto skip2;
					if (colorClusters) SetColorTracklets();
				}
				drawVertices(GLpoints[iSlice][4][iCol], GL_POINTS);

				if (drawTracks)
				{
					if (excludeClusters) goto skip2;
					if (colorClusters) SetColorTracks();
				}
				drawVertices(GLpoints[iSlice][5][iCol], GL_POINTS);

			skip2:
				if (drawGlobalTracks)
				{
					if (excludeClusters) goto skip3;
					if (colorClusters) SetColorGlobalTracks();
				}
				else
				{
					SetColorClusters();
				}
				drawVertices(GLpoints[iSlice][6][iCol], GL_POINTS);
				SetColorClusters();

				if (drawFinal)
				{
					if (excludeClusters) goto skip3;
					if (colorClusters) SetColorFinal();
				}
				drawVertices(GLpoints[iSlice][7][iCol], GL_POINTS);
			skip3:;
				if (showCollision != -1) break;
			}
		}

		if (!excludeClusters)
		{
			if (drawInitLinks)
			{
				SetColorInitLinks();
				drawVertices(glDLlines[iSlice][0], GL_LINES);
			}
			if (drawLinks)
			{
				SetColorLinks();
				drawVertices(glDLlines[iSlice][1], GL_LINES);
			}
			if (drawSeeds)
			{
				SetColorSeeds();
				drawVertices(glDLlines[iSlice][2], GL_LINE_STRIP);
			}
			if (drawTracklets)
			{
				SetColorTracklets();
				drawVertices(glDLlines[iSlice][3], GL_LINE_STRIP);
			}
			if (drawTracks)
			{
				SetColorTracks();
				drawVertices(glDLlines[iSlice][4], GL_LINE_STRIP);
			}
			if (drawGlobalTracks)
			{
				SetColorGlobalTracks();
				drawVertices(glDLlines[iSlice][5], GL_LINE_STRIP);
			}
			for (int iCol = 0;iCol < nCollisions;iCol++)
			{
				if (showCollision != -1) iCol = showCollision;
				if (drawFinal)
				{
					SetColorFinal();
					if (colorCollisions) SetCollisionColor(iCol);
					//if (!drawClusters) drawVertices(GLpoints[iSlice][7][iCol], GL_POINTS);
					drawVertices(glDLfinal[iSlice][iCol], GL_LINE_STRIP);
				}
				if (markClusters)
				{
					SetColorMarked();
					drawVertices(GLpoints[iSlice][8][iCol], GL_POINTS);
				}
				if (showCollision != -1) break;
			}
		}
	}
	CHKERR(glDisableClientState(GL_VERTEX_ARRAY));
	

	
	framesDone++;
	framesDoneFPS++;
	double time = timerFPS.GetCurrentElapsedTime();
	char info[1024];
	float fps = (double) framesDoneFPS / time;
	sprintf(info, "FPS: %6.2f (Slice: %d, 1:Clusters %d, 2:Prelinks %d, 3:Links %d, 4:Seeds %d, 5:Tracklets %d, 6:Tracks %d, 7:GTracks %d, 8:Merger %d) (%d frames, %d draw calls) "
		"(X %1.2f Y %1.2f Z %1.2f / R %1.2f Phi %1.1f Theta %1.1f) / Yaw %1.1f Pitch %1.1f Roll %1.1f)",
		fps, drawSlice, drawClusters, drawInitLinks, drawLinks, drawSeeds, drawTracklets, drawTracks, drawGlobalTracks, drawFinal, framesDone, drawCalls,
		xyz[0], xyz[1], xyz[2], rphitheta[0], rphitheta[1] * 180 / M_PI, rphitheta[2] * 180 / M_PI, angle[1] * 180 / M_PI, angle[0] * 180 / M_PI, angle[2] * 180 / M_PI);
	if (time > 1.)
	{
		if (printInfoText & 2) printf("%s\n", info);
		if (fpsscaleadjust++) fpsscale = 60 / fps;
		timerFPS.ResetStart();
		framesDoneFPS = 0;
	}		
	
	if (printInfoText & 1) showInfo(info);

//Free event
#ifdef R__WIN32
	ReleaseSemaphore(semLockDisplay, 1, NULL);
#else
	pthread_mutex_unlock(&semLockDisplay);
#endif

	return true; // Keep Going
}

void DoScreenshot(char *filename, int SCALE_X, unsigned char **mixBuffer = NULL, float mixFactor = 0.)
{
	int SCALE_Y = SCALE_X;

	if (mixFactor < 0.f) mixFactor = 0.f;
	if (mixFactor > 1.f) mixFactor = 1.f;

	float tmpPointSize = pointSize;
	float tmpLineWidth = lineWidth;
	pointSize *= (float) (SCALE_X + SCALE_Y) / 2.;
	lineWidth *= (float) (SCALE_X + SCALE_Y) / 2.;

	GLint view[4], viewold[4];
	glGetIntegerv(GL_VIEWPORT, viewold);
	glGetIntegerv(GL_VIEWPORT, view);
	view[2] *= SCALE_X;
	view[3] *= SCALE_Y;
	unsigned char *pixels = (unsigned char *) malloc(4 * view[2] * view[3]);

	if (SCALE_X != 1 || SCALE_Y != 1)
	{
		memset(pixels, 0, 4 * view[2] * view[3]);
		unsigned char *pixels2 = (unsigned char *) malloc(4 * view[2] * view[3]);
		for (int i = 0; i < SCALE_X; i++)
		{
			for (int j = 0; j < SCALE_Y; j++)
			{
				glViewport(-i * viewold[2], -j * viewold[3], view[2], view[3]);

				DrawGLScene();
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glReadBuffer(GL_BACK);
				glReadPixels(0, 0, view[2], view[3], GL_RGBA, GL_UNSIGNED_BYTE, pixels2);
				for (int k = 0; k < viewold[2]; k++)
				{
					for (int l = 0; l < viewold[3]; l++)
					{
						pixels[((j * viewold[3] + l) * view[2] + i * viewold[2] + k) * 4] = pixels2[(l * view[2] + k) * 4 + 2];
						pixels[((j * viewold[3] + l) * view[2] + i * viewold[2] + k) * 4 + 1] = pixels2[(l * view[2] + k) * 4 + 1];
						pixels[((j * viewold[3] + l) * view[2] + i * viewold[2] + k) * 4 + 2] = pixels2[(l * view[2] + k) * 4];
						pixels[((j * viewold[3] + l) * view[2] + i * viewold[2] + k) * 4 + 3] = 0;
					}
				}
			}
		}
		free(pixels2);
	}
	else
	{
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, view[2], view[3], GL_BGRA, GL_UNSIGNED_BYTE, pixels);
	}

	if (mixBuffer)
	{
		if (*mixBuffer == NULL)
		{
			*mixBuffer = (unsigned char *) malloc(4 * view[2] * view[3]);
			memcpy(*mixBuffer, pixels, 4 * view[2] * view[3]);
		}
		else
		{
			for (int i = 0; i < 4 * view[2] * view[3]; i++)
			{
				pixels[i] = (*mixBuffer)[i] = (mixFactor * pixels[i] + (1.f - mixFactor) * (*mixBuffer)[i]);
			}
		}
	}

	if (filename)
	{
		FILE *fp = fopen(filename, "w+b");

		BITMAPFILEHEADER bmpFH;
		BITMAPINFOHEADER bmpIH;
		memset(&bmpFH, 0, sizeof(bmpFH));
		memset(&bmpIH, 0, sizeof(bmpIH));

		bmpFH.bfType = 19778; //"BM"
		bmpFH.bfSize = sizeof(bmpFH) + sizeof(bmpIH) + 4 * view[2] * view[3];
		bmpFH.bfOffBits = sizeof(bmpFH) + sizeof(bmpIH);

		bmpIH.biSize = sizeof(bmpIH);
		bmpIH.biWidth = view[2];
		bmpIH.biHeight = view[3];
		bmpIH.biPlanes = 1;
		bmpIH.biBitCount = 32;
		bmpIH.biCompression = BI_RGB;
		bmpIH.biSizeImage = view[2] * view[3] * 4;
		bmpIH.biXPelsPerMeter = 5670;
		bmpIH.biYPelsPerMeter = 5670;

		fwrite(&bmpFH, 1, sizeof(bmpFH), fp);
		fwrite(&bmpIH, 1, sizeof(bmpIH), fp);
		fwrite(pixels, view[2] * view[3], 4, fp);
		fclose(fp);
	}
	free(pixels);

	glViewport(0, 0, viewold[2], viewold[3]);
	pointSize = tmpPointSize;
	lineWidth = tmpLineWidth;
	DrawGLScene();
}

const char* HelpText[] = {
	"[n] / [SPACE]            Next event", 
	"[q] / [Q] / [ESC]        Quit", 
	"[r]                      Reset Display Settings", 
	"[l]                      Draw single slice (next slice)", 
	"[k]                      Draw single slice (previous slice)", 
	"[J]                      Draw related slices (same plane in phi)", 
	"[z] / [U]                Show splitting of TPC in slices by extruding volume, [U] resets", 
	"[y] / [Y] / [X] / [N]    Start Animation / Add animation point / Reset / Cycle mode", 
	"[g]                      Draw Grid", 
	"[i]                      Project onto XY-plane", 
	"[x]                      Exclude Clusters used in the tracking steps enabled for visualization ([1]-[8])", 
	"[<]                      Exclude rejected tracks", 
	"[c]                      Mark flagged clusters (splitPad = 0x1, splitTime = 0x2, edge = 0x4, singlePad = 0x8, rejectDistance = 0x10, rejectErr = 0x20", 
	"[C]                      Colorcode clusters of different collisions", 
	"[v]                      Hide rejected clusters from tracks", 
	"[b]                      Hide all clusters not belonging or related to matched tracks", 
	"[1]                      Show Clusters", 
	"[2]                      Show Links that were removed", 
	"[3]                      Show Links that remained in Neighbors Cleaner", 
	"[4]                      Show Seeds (Start Hits)", 
	"[5]                      Show Tracklets", 
	"[6]                      Show Tracks (after Tracklet Selector)", 
	"[7]                      Show Global Track Segments", 
	"[8]                      Show Final Merged Tracks (after Track Merger)", 
	"[j]                      Show global tracks as additional segments of final tracks", 
	"[m]                      Reorder clusters of merged tracks before showing them geometrically", 
	"[t]                      Take Screenshot", 
	"[T]                      Change screenshot resolution (scaling factor)",
	"[S] / [A] / [D]          Enable or disable smoothing of points / smoothing of lines / depth buffer",
	"[W] / [V]                Enable / disable anti-aliasing / VSync",
	"[F]                      Switch fullscreen",
	"[I]                      Enable / disable GL indirect draw",
	"[o]                      Save current camera position", 
	"[p]                      Restore camera position", 
	"[h]                      Print Help", 
	"[H]                      Show info texts",
	"[w] / [s] / [a] / [d]    Zoom / Strafe Left and Right", 
	"[pgup] / [pgdn]          Strafe Up and Down",
	"[e] / [f]                Rotate", 
	"[+] / [-]                Make points thicker / fainter (Hold SHIFT for lines)", 
	"[MOUSE 1]                Look around", 
	"[MOUSE 2]                Shift camera", 
	"[MOUSE 1+2]              Zoom / Rotate", 
	"[SHIFT]                  Slow Zoom / Move / Rotate",
	"[ALT] / [CTRL] / [M]     Focus camera on origin / orient y-axis upwards (combine with [SHIFT] to lock) / Cycle through modes",
	"[1] ... [8] / [V]        Enable display of clusters, preseeds, seeds, starthits, tracklets, tracks, global tracks, merged tracks / Show assigned clusters in colors"
};

void PrintHelp()
{
	infoHelpTimer.ResetStart();
	for (unsigned int i = 0;i < sizeof(HelpText) / sizeof(HelpText[0]);i++) printf("%s\n", HelpText[i]);
}

void HandleKeyRelease(int wParam)
{
	keys[wParam] = false;
	
	if (wParam >= 'A' && wParam <= 'Z')
	{
		if (keysShift[wParam]) wParam &= ~(int) ('a' ^ 'A');
		else wParam |= (int) ('a' ^ 'A');
	}

	if (wParam == 13 || wParam == 'n')
	{
		exitButton = 1;
		SetInfo("Showing next event");
	}
	else if (wParam == 27 || wParam == 'q' || wParam == 'Q')
	{
		exitButton = 2;
		SetInfo("Exiting");
	}
	else if (wParam == 'r')
	{
		resetScene = 1;
		SetInfo("View reset");
	}
	else if (wParam == 'l')
	{
		if (drawSlice >= (drawRelatedSlices ? (fgkNSlices / 4 - 1) : (fgkNSlices - 1)))
		{
			drawSlice = -1;
			SetInfo("Showing all slices");
		}
		else
		{
			drawSlice++;
			SetInfo("Showing slice %d", drawSlice);
		}
	}
	else if (wParam == KEY_ALT && keysShift[KEY_ALT])
	{
		camLookOrigin ^= 1;
		cameraMode = camLookOrigin + 2 * camYUp;
		SetInfo("Camera locked on origin: %s", camLookOrigin ? "enabled" : "disabled");
	}
	else if (wParam == KEY_CTRL && keysShift[KEY_CTRL])
	{
		camYUp ^= 1;
		cameraMode = camLookOrigin + 2 * camYUp;
		SetInfo("Camera locked on y-axis facing upwards: %s", camYUp ? "enabled" : "disabled");
	}
	else if (wParam == 'm')
	{
		cameraMode++;
		if (cameraMode == 4) cameraMode = 0;
		camLookOrigin = cameraMode & 1;
		camYUp = cameraMode & 2;
		const char* modeText[] = {"Descent (free movement)", "Focus locked on origin (y-axis forced upwards)", "Spectator (y-axis forced upwards)", "Focus locked on origin (with free rotation)"};
		SetInfo("Camera mode %d: %s", cameraMode, modeText[cameraMode]);
	}
	else if (wParam == KEY_ALT)
	{
		keys[KEY_CTRL] = false; //Release CTRL with alt, to avoid orienting along y automatically!
	}
	else if (wParam == 'k')
	{
		if (drawSlice <= -1)
		{
			drawSlice = drawRelatedSlices ? (fgkNSlices / 4 - 1) : (fgkNSlices - 1);
			SetInfo("Showing all slices");
		}
		else
		{
			drawSlice--;
			SetInfo("Showing slice %d", drawSlice);
		}
	}
	else if (wParam == 'L')
	{
		if (showCollision >= nCollisions - 1)
		{
			showCollision = -1;
			SetInfo("Showing all collisions");
		}
		else
		{
			showCollision++;
			SetInfo("Showing collision %d", showCollision);
		}
	}
	else if (wParam == 'F')
	{
		SwitchFullscreen();
		SetInfo("Toggling full screen");
	}
	else if (wParam == 'K')
	{
		if (showCollision <= -1)
		{
			showCollision = nCollisions - 1;
			SetInfo("Showing all collisions");
		}
		else
		{
			showCollision--;
			SetInfo("Showing collision %d", showCollision);
		}
	}
	else if (wParam == 'H')
	{
		printInfoText += 1;
		printInfoText &= 3;
		SetInfo("Info text display - console: %s, onscreen %s", (printInfoText & 2) ? "enabled" : "disabled", (printInfoText & 1) ? "enabled" : "disabled");
	}
	else if (wParam == 'J')
	{
		drawRelatedSlices ^= 1;
		SetInfo("Drawing of related slices %s", drawRelatedSlices ? "enabled" : "disabled");
	}
	else if (wParam == 'j')
	{
		separateGlobalTracks ^= 1;
		SetInfo("Seperated display of global tracks %s", separateGlobalTracks ? "enabled" : "disabled");
		updateDLList = true;
	}
	else if (wParam == 'm')
	{
		reorderFinalTracks ^= 1;
		SetInfo("Reordering hits of final tracks %s", reorderFinalTracks ? "enabled" : "disabled");
		updateDLList = true;
	}
	else if (wParam == 'c')
	{
		if (markClusters == 0) markClusters = 1;
		else if (markClusters >= 0x20) markClusters = 0;
		else markClusters <<= 1;
		SetInfo("Cluster flag highlight mask set to %d (%s)", markClusters, markClusters == 0 ? "off" : markClusters == 1 ? "split pad" : markClusters == 2 ? "split time" : markClusters == 4 ? "edge" : markClusters == 8 ? "singlePad" : markClusters == 0x10 ? "reject distance" : "reject error");
		updateDLList = true;
	}
	else if (wParam == 'C')
	{
		colorCollisions ^= 1;
		SetInfo("Color coding of collisions %s", colorCollisions ? "enabled" : "disabled");
	}
	else if (wParam == 'V')
	{
		colorClusters ^= 1;
		SetInfo("Color coding for seed / trrack attachmend %s", colorClusters ? "enabled" : "disabled");
	}
	else if (wParam == 'P')
	{
		propagateTracks += 1;
		if (propagateTracks == 4) propagateTracks = 0;
		const char* infoText[] = {"Hits connected", "Hits connected and propagated to vertex", "Reconstructed track propagated inwards and outwards", "Monte Carlo track"};
		SetInfo("Display of track propagation: %s", infoText[propagateTracks]);
		updateDLList = true;
	}
	else if (wParam == 'v')
	{
		hideRejectedClusters ^= 1;
		SetInfo("Rejected clusters are %s", hideRejectedClusters ? "hidden" : "shown");
		updateDLList = true;
	}
	else if (wParam == 'b')
	{
		hideUnmatchedClusters ^= 1;
		SetInfo("Unmatched clusters are %s", hideRejectedClusters ? "hidden" : "shown");
		updateDLList = true;
	}
	else if (wParam == 'i')
	{
		projectxy ^= 1;
		SetInfo("Projection onto xy plane %s", projectxy ? "enabled" : "disabled");
		updateDLList = true;
	}
	else if (wParam == 'S')
	{
		smoothPoints ^= true;
		SetInfo("Smoothing of points %s", smoothPoints ? "enabled" : "disabled");
	}
	else if (wParam == 'A')
	{
		smoothLines ^= true;
		SetInfo("Smoothing of lines %s", smoothLines ? "enabled" : "disabled");
	}
	else if (wParam == 'D')
	{
		depthBuffer ^= true;
		GLint depthBits;
		glGetIntegerv(GL_DEPTH_BITS, &depthBits);
		SetInfo("Depth buffer (z-buffer, %d bits) %s", depthBits, depthBuffer ? "enabled" : "disabled");
		setDepthBuffer();
	}
	else if (wParam == 'W')
	{
		drawQualityMSAA ^= 1;
		setQuality();
		SetInfo("Multisampling anti-aliasing: %s", drawQualityMSAA ? "enabled" : "disabled");
	}
	else if (wParam == 'V')
	{
		drawQualityVSync ^= 1;
		SetVSync(drawQualityVSync);
		SetInfo("VSync: %s", drawQualityVSync ? "enabled" : "disabled");
	}
	else if (wParam == 'I')
	{
		useGLIndirectDraw ^= true;
		SetInfo("OpenGL Indirect Draw %s", useGLIndirectDraw ? "enabled" : "disabled");
		updateDLList = true;
	}
	else if (wParam == 'z')
	{
		updateDLList = true;
		Xadd += 60;
		Zadd += 60;
		SetInfo("TPC sector separation: %f %f", Xadd, Zadd);
	}
	else if (wParam == 'u')
	{
		updateDLList = true;
		Xadd -= 60;
		Zadd -= 60;
		if (Zadd < 0 || Xadd < 0) Zadd = Xadd = 0;
		SetInfo("TPC sector separation: %f %f", Xadd, Zadd);
	}
	else if (wParam == 'y')
	{
		if (animateVectors[0].size() > 1)
		{
			startAnimation();
			SetInfo("Starting animation");
		}
		else
		{
			SetInfo("Insufficient animation points to start animation");
		}
	}
	else if (wParam == 'Y')
	{
		setAnimationPoint();
		SetInfo("Added animation point (%d points, %6.2f seconds)", (int) animateVectors[0].size(), animateVectors[0].back());
	}
	else if (wParam == 'X')
	{
		resetAnimation();
		SetInfo("Reset animation points");
	}
	else if (wParam == 'N')
	{
		animationMode++;
		if (animationMode == 4) animationMode = 0;
		resetAnimation();
		SetInfo("Animation mode %d - Position: %s, Direction: %s", animationMode, animationMode & 2 ? "Sperical" : "Cartesian", animationMode & 1 ? "Euler angles" : "Quaternion");
	}
	else if (wParam == 'g')
	{
		drawGrid ^= 1;
		SetInfo("Fast Cluster Search Grid %s", drawGrid ? "shown" : "hidden");
	}
	else if (wParam == 'x')
	{
		excludeClusters ^= 1;
		SetInfo(excludeClusters ? "Clusters of selected category are excluded from display" : "Clusters are shown");
	}
	else if (wParam == '<')
	{
		hideRejectedTracks ^= 1;
		SetInfo("Rejected tracks are %s", hideRejectedTracks ? "hidden" : "shown");
		updateDLList = true;
	}

	else if (wParam == '1')
	{
		drawClusters ^= 1;
	}
	else if (wParam == '2')
	{
		drawInitLinks ^= 1;
		updateDLList = true;
	}
	else if (wParam == '3')
	{
		drawLinks ^= 1;
	}
	else if (wParam == '4')
	{
		drawSeeds ^= 1;
	}
	else if (wParam == '5')
	{
		drawTracklets ^= 1;
	}
	else if (wParam == '6')
	{
		drawTracks ^= 1;
	}
	else if (wParam == '7')
	{
		drawGlobalTracks ^= 1;
	}
	else if (wParam == '8')
	{
		drawFinal ^= 1;
	}
	else if (wParam == 't')
	{
		printf("Taking screenshot\n");
		static int nScreenshot = 1;
		char fname[32];
		sprintf(fname, "screenshot%d.bmp", nScreenshot++);
		DoScreenshot(fname, screenshot_scale);
		SetInfo("Taking screenshot (%s)", fname);
	}
	else if (wParam == 'T')
	{
		screenshot_scale += 1;
		if (screenshot_scale == 5) screenshot_scale = 1;
		SetInfo("Screenshot scaling factor set to %d", screenshot_scale);
	}
	else if (wParam == 'o')
	{
		FILE *ftmp = fopen("glpos.tmp", "w+b");
		if (ftmp)
		{
			int retval = fwrite(&currentMatrix[0], sizeof(GLfloat), 16, ftmp);
			if (retval != 16)
			{
				printf("Error writing position to file\n");
			}
			else
			{
				printf("Position stored to file\n");
			}
			fclose(ftmp);
		}
		else
		{
			printf("Error opening file\n");
		}
		SetInfo("Storing camera position to file");
	}
	else if (wParam == 'p')
	{
		GLfloat tmp[16];
		FILE *ftmp = fopen("glpos.tmp", "rb");
		if (ftmp)
		{
			int retval = fread(&tmp[0], sizeof(GLfloat), 16, ftmp);
			if (retval == 16)
			{
				glMatrixMode(GL_MODELVIEW);
				glLoadMatrixf(tmp);
				glGetFloatv(GL_MODELVIEW_MATRIX, currentMatrix);
				printf("Position read from file\n");
			}
			else
			{
				printf("Error reading position from file\n");
			}
			fclose(ftmp);
		}
		else
		{
			printf("Error opening file\n");
		}
		SetInfo("Loading camera position from file");
	}
	else if (wParam == 'h')
	{
		PrintHelp();
		SetInfo("Showing help text");
	}
}

void showInfo(const char* info)
{
	GLfloat tmp[16];
	glGetFloatv(GL_PROJECTION_MATRIX, tmp);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.f, screen_width, 0.f, screen_height);
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2f(40.f, 40.f);
	OpenGLPrint(info);
	if (infoText2Timer.IsRunning())
	{
		if (infoText2Timer.GetCurrentElapsedTime() >= 5) infoText2Timer.Reset();
		glRasterPos2f(40.f, 20.f);
		OpenGLPrint(infoText2);		
	}
	if (infoHelpTimer.IsRunning())
	{
		if (infoHelpTimer.GetCurrentElapsedTime() >= 5) infoHelpTimer.Reset();
		for (unsigned int i = 0;i < sizeof(HelpText) / sizeof(HelpText[0]);i++)
		{
			glRasterPos2f(40.f, screen_height - 35 - 20 * (1 + i));
			OpenGLPrint(HelpText[i]);					
		}
	}
	glLoadMatrixf(tmp);
}	

void HandleSendKey()
{
	if (sendKey)
	{
		//fprintf(stderr, "sendKey %d '%c'\n", sendKey, (char) sendKey);

		bool shifted = sendKey >= 'A' && sendKey <= 'Z';
		if (sendKey >= 'a' && sendKey <= 'z') sendKey ^= 'a' ^ 'A';
		bool oldShift = keysShift[sendKey];
		keysShift[sendKey] = shifted;
		HandleKeyRelease(sendKey);
		keysShift[sendKey] = oldShift;
		sendKey = 0;
	}
}

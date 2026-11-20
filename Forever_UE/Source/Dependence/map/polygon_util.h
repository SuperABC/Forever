#pragma once

#include <vector>


// 2D多边形小工具，设计见polygon_util.md。多边形用扁平数组xy={x0,y0,x1,y1,...}表示，首尾不重复，
// 隐含最后一点连回第一点。凹多边形可用，输入不能自交。所有函数都是纯函数，只读入参。

// 有符号面积(shoelace)：>0 表示按"x向右、y向下"的屏幕坐标看是顺时针，<0 反之。
// 本项目约定"规整后"的多边形有符号面积>0(见EnsurePositiveArea)。
float PolygonSignedArea(const std::vector<float>& xy);

// 面积为负时把顶点顺序倒过来，让有符号面积>0。返回是否翻转过。
bool EnsurePositiveArea(std::vector<float>& xy);

// 包围盒。
void PolygonBounds(const std::vector<float>& xy, float& minX, float& minY, float& maxX, float& maxY);

// 点在多边形内(射线法，边界上的点按"内"处理)。
bool PointInPolygon(const std::vector<float>& xy, float x, float y);

// 点到线段(ax,ay)-(bx,by)的最短距离，(cx,cy)输出线段上的最近点(垂足在段外时是端点)。
float ClosestPointOnSegment(float ax, float ay, float bx, float by, float px, float py, float& cx, float& cy);

// 点到多边形边界的最短距离(无符号)。
float DistanceToPolygonBoundary(const std::vector<float>& xy, float x, float y);

// 点到多边形的有符号距离：内部为正(到边界的距离)，外部为负。圆内切搜索用。
float SignedDistanceToPolygon(const std::vector<float>& xy, float x, float y);

// 线段(x0,y0)-(x1,y1)整体是否落在多边形内(允许贴着边界/擦过顶点，不允许穿出去)：
// 求线段与每条边的所有交点把线段切成若干段，每段的中点都必须在多边形内。
bool SegmentInsidePolygon(const std::vector<float>& xy, float x0, float y0, float x1, float y1);

// 多边形是否自交(非相邻两条边相交，含端点相碰)。
bool PolygonSelfIntersects(const std::vector<float>& xy);

// 两线段是否相交(含端点相碰、共线重叠)。
bool SegmentsIntersect(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy);

// 耳切三角化。输出三角形顶点下标(3个一组，下标指向顶点序号而不是xy下标)，三角形面积之和等于多边形面积。
// 顶点少于3个或退化时返回false。内部会自己判断绕向，不要求先EnsurePositiveArea。
bool TriangulatePolygon(const std::vector<float>& xy, std::vector<int>& triangles);

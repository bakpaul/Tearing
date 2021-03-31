#pragma once
#include "TearingEngine.h"

#include <sofa/core/visual/VisualParams.h>
#include <sofa/helper/types/RGBAColor.h>
#include <sofa/helper/ColorMap.h>
#include <SofaBaseTopology/TopologyData.inl>

//#include <SofaBaseTopology/TriangleSetGeometryAlgorithms.h>


namespace sofa::component::engine
{
template <class DataTypes>
TearingEngine<DataTypes>::TearingEngine()
    : input_position(initData(&input_position, "input_position", "Input position"))
    , d_initArea(initData(&d_initArea, "initArea", "list of initial area"))
    , d_seuil(initData(&d_seuil, 0.1, "seuil", "threshold value for area"))
    , d_triangleList_TEST(initData(&d_triangleList_TEST, "triangleList_TEST", "valeur TEST a supprimer"))
	, l_topology(initLink("topology", "link to the topology container"))
	, m_topology(nullptr)
    , m_triangleGeo(nullptr)
    , m_triangularFEM(nullptr)
    , showChangedTriangle( initData(&showChangedTriangle,true,"showChangedTriangle", "Flag activating rendering of changed triangle"))
    , d_triangleInfoTearing(initData(&d_triangleInfoTearing, "triangleInfoTearing", "Internal triangle data"))
    , d_triangleFEMInfo(initData(&d_triangleFEMInfo, "triangleFEMInfo", "Internal triangle data"))

    , d_barycoef1(initData(&d_barycoef1, "barycoef1", "braycoef1"))
    , d_barycoef2(initData(&d_barycoef2, "barycoef2", "braycoef2"))
    , d_barycoef3(initData(&d_barycoef3, "barycoef3", "braycoef3"))

{
    addInput(&input_position);
    addInput(&d_seuil);
    addOutput(&d_triangleInfoTearing);
    addOutput(&d_triangleFEMInfo);
    addOutput(&d_triangleList_TEST);
    p_drawColorMap = new helper::ColorMap(256, "Blue to Red");
}

template <class DataTypes>
void TearingEngine<DataTypes>::init()
{
    if (l_topology.empty())
    {
        msg_info() << "link to Topology container should be set to ensure right behavior. First Topology found in current context will be used.";
        l_topology.set(this->getContext()->getMeshTopologyLink());
    }

    m_topology = l_topology.get();
    msg_info() << "Topology path used: '" << l_topology.getLinkedPath() << "'";

    if (m_topology == nullptr)
    {
        msg_error() << "No topology component found at path: " << l_topology.getLinkedPath() << ", nor in current context: " << this->getContext()->name;
        sofa::core::objectmodel::BaseObject::d_componentState.setValue(sofa::core::objectmodel::ComponentState::Invalid);
        return;
    }

    m_topology->getContext()->get(m_triangleGeo);
    if (!m_triangleGeo)
    {
        msg_error() << "Missing component: Unable to get TriangleSetGeometryAlgorithms from the current context.";
        sofa::core::objectmodel::BaseObject::d_componentState.setValue(sofa::core::objectmodel::ComponentState::Invalid);
        return;
    }

    m_topology->getContext()->get(m_triangularFEM);
    if (!m_triangularFEM)
    {
        msg_error() << "Missing component: Unable to get TriangleSetGeometryAlgorithms from the current context.";
        sofa::core::objectmodel::BaseObject::d_componentState.setValue(sofa::core::objectmodel::ComponentState::Invalid);
        return;
    }

    initComputeArea();
    computeArea();
    updateTriangleInformation();
    triangleOverThreshold();


}

template <class DataTypes>
void TearingEngine<DataTypes>::reinit()
{
	update();
}

template <class DataTypes>
void TearingEngine<DataTypes>::doUpdate()
{
    computeArea();
    updateTriangleInformation();
    triangleOverThreshold(); 
    //test computeRestTriangleArea
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    helper::ReadAccessor< Data<VecCoord> > x(input_position);
    helper::WriteAccessor< Data<defaulttype::Vec<3, Real>> > barycoef1(d_barycoef1);
    helper::WriteAccessor< Data<defaulttype::Vec<3, Real>> > barycoef2(d_barycoef2);
    helper::WriteAccessor< Data<defaulttype::Vec<3, Real>> > barycoef3(d_barycoef3);
    sofa::helper::vector< double > point1, point2, point3;
    Element triangle = triangleList[0];
    point1 = m_triangleGeo->computeTriangleBarycoefs(0, x[triangle[0]]);
    point2 = m_triangleGeo->computeTriangleBarycoefs(0, x[triangle[1]]);
    point3 = m_triangleGeo->computeTriangleBarycoefs(0, (x[triangle[0]]+ x[triangle[1]]+ x[triangle[2]])/3);
    for (unsigned int i = 0; i < 3; i++)
    {
        barycoef1[i] = point1[i];
        barycoef2[i] = point2[i];
        barycoef3[i] = point3[i];
    }
}


template <class DataTypes>
void TearingEngine<DataTypes>::draw(const core::visual::VisualParams* vparams)
{
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    helper::ReadAccessor< Data<VecCoord> > x(input_position);
    helper::WriteAccessor< Data<vector<TriangleInformation>> > triangleInf(d_triangleInfoTearing); //ne fonctionne pas en ReadAccessor
    helper::ReadAccessor< Data<vector<double>> > initArea(d_initArea);

    double minDeltaArea = std::numeric_limits<double>::max();
    double maxDeltaArea = 0.0;

    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        TriangleInformation* tinfo = &triangleInf[i];
        Real deltaArea = abs(initArea[i] - tinfo->area);

        if (deltaArea < minDeltaArea)
            minDeltaArea = deltaArea;
        if (deltaArea > maxDeltaArea)
            maxDeltaArea = deltaArea;
    }

    std::vector<sofa::defaulttype::Vector3> vertices;
    std::vector<sofa::helper::types::RGBAColor> colorVector;
    helper::ColorMap::evaluator<double> evalColor = p_drawColorMap->getEvaluator(0.0, maxDeltaArea);

    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        Element triangle = triangleList[i];
        TriangleInformation* tinfo = &triangleInf[i];

        Index a = triangle[0];
        Index b = triangle[1];
        Index c = triangle[2];

        Coord Pa = x[a];
        Coord Pb = x[b];
        Coord Pc = x[c];

        colorVector.push_back(evalColor(abs(initArea[i] - tinfo->area)));
        vertices.push_back(Pa);
        colorVector.push_back(evalColor(abs(initArea[i] - tinfo->area)));
        vertices.push_back(Pb);
        colorVector.push_back(evalColor(abs(initArea[i] - tinfo->area)));
        vertices.push_back(Pc);
    }
    vparams->drawTool()->drawTriangles(vertices, colorVector);
    vertices.clear();
    colorVector.clear();
}



// --------------------------------------------------------------------------------------
// --- Computation methods
// --------------------------------------------------------------------------------------
template <class DataTypes>
void TearingEngine<DataTypes>::initComputeArea()
{
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    helper::ReadAccessor< Data<VecCoord> > x(input_position);
    helper::WriteAccessor< Data<vector<double>> > initArea(d_initArea);
    helper::WriteAccessor< Data<vector<TriangleInformation>> > triangleInf(d_triangleInfoTearing);
    if (initArea.size() != triangleList.size())
        initArea.resize(triangleList.size());
    if (triangleInf.size() != triangleList.size())
        triangleInf.resize(triangleList.size());

    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        Element triangle = triangleList[i];
        TriangleInformation* tinfo = &triangleInf[i];

        Index a = triangle[0];
        Index b = triangle[1];
        Index c = triangle[2];

        Coord Pa = x[a];
        Coord Pb = x[b];
        Coord Pc = x[c];

        Real determinant;

        Coord ab_cross_ac = cross(Pb - Pa, Pc - Pa);
        determinant = ab_cross_ac.norm();
        tinfo->area = determinant * 0.5f;
        initArea[i] = tinfo->area;
    }
}

template <class DataTypes>
void TearingEngine<DataTypes>::computeArea()
{
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    helper::ReadAccessor< Data<VecCoord> > x(input_position);
    helper::WriteAccessor< Data<vector<TriangleInformation>> > triangleInf(d_triangleInfoTearing);
    if(triangleInf.size() != triangleList.size() )
    {
        triangleInf.resize(triangleList.size());
    }
    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        Element triangle = triangleList[i];
        TriangleInformation* tinfo = &triangleInf[i];
        Index a = triangle[0];
        Index b = triangle[1];
        Index c = triangle[2];

        Coord Pa = x[a];
        Coord Pb = x[b];
        Coord Pc = x[c];

        Real determinant;
        Coord ab_cross_ac = cross(Pb - Pa, Pc - Pa);
        determinant = ab_cross_ac.norm();
        tinfo->area = determinant * 0.5f;
    }
}


template <class DataTypes>
void TearingEngine<DataTypes>::triangleOverThreshold()
{
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    helper::ReadAccessor< Data<double> > threshold(d_seuil);
    helper::WriteAccessor< Data<VecElement> > TEST(d_triangleList_TEST);
    helper::WriteAccessor< Data<vector<TriangleInformation>> > triangleInf(d_triangleInfoTearing);
    TEST.clear();
    //ne pas oublier � la fin d'un step de clear cette liste sinon on accumule les triangles
    Index max = 0;
    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        TriangleInformation* tinfo = &triangleInf[i];
        if (tinfo->area >= threshold)
        {
            TEST.push_back(triangleList[i]);
            if (tinfo->area > triangleInf[max].area)
                max = i;
        }         
    }
}

template <class DataTypes>
void TearingEngine<DataTypes>::updateTriangleInformation()
{
    //update triangleInformation
    VecElement triangleList;
    triangleList = m_topology->getTriangles();
    d_triangleFEMInfo = m_triangularFEM->triangleInfo.getValue();
    helper::WriteAccessor< Data<vector<TriangleInformation>> > triangleInf(d_triangleInfoTearing);
    helper::WriteAccessor< Data<VecTriangleFEMInformation> > triangleFEMInf(d_triangleFEMInfo);

    if (triangleInf.size() != triangleList.size())
    {
        triangleInf.resize(triangleList.size());
    }
    if (triangleFEMInf.size() != triangleList.size())
    {
        triangleFEMInf.resize(triangleList.size());
    }

    for (unsigned int i = 0; i < triangleList.size(); i++)
    {
        TriangleFEMInformation tFEMinfo = triangleFEMInf[i];
        TriangleInformation* tinfo = &triangleInf[i];

        //tinfo->area =  tFEMinfo.area;;
        tinfo->stress = tFEMinfo.stress;
    }
}

} //namespace sofa::component::engine





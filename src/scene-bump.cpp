/*
 * Copyright © 2010-2011 Linaro Limited
 *
 * This file is part of the glmark2 OpenGL (ES) 2.0 benchmark.
 *
 * glmark2 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * glmark2 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * glmark2.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Alexandros Frantzis (glmark2)
 */
#include "scene.h"
#include "log.h"
#include "mat.h"
#include "stack.h"
#include <cmath>

SceneBump::SceneBump(Canvas &pCanvas) :
    Scene(pCanvas, "bump")
{
    mOptions["bump-render"] = Scene::Option("bump-render", "off",
                                            "How to render bumps [off, normals, high-poly]");
}

SceneBump::~SceneBump()
{
}

int SceneBump::load()
{
    mRotationSpeed = 36.0f;

    mRunning = false;

    return 1;
}

void SceneBump::unload()
{
}

void
SceneBump::setup_high_polygon()
{
    static const std::string vtx_shader_filename(GLMARK_DATA_PATH"/shaders/light-advanced.vert");
    static const std::string frg_shader_filename(GLMARK_DATA_PATH"/shaders/light-advanced.frag");
    Model model;

    if(!model.load_3ds(GLMARK_DATA_PATH"/models/asteroid-high.3ds"))
        return;

    model.calculate_normals();

    /* Tell the converter that we only care about position and normal attributes */
    std::vector<std::pair<Model::AttribType, int> > attribs;
    attribs.push_back(std::pair<Model::AttribType, int>(Model::AttribTypePosition, 3));
    attribs.push_back(std::pair<Model::AttribType, int>(Model::AttribTypeNormal, 3));

    model.convert_to_mesh(mMesh, attribs);

    if (!Scene::load_shaders_from_files(mProgram, vtx_shader_filename,
                                        frg_shader_filename))
    {
        return;
    }

    std::vector<GLint> attrib_locations;
    attrib_locations.push_back(mProgram.getAttribIndex("position"));
    attrib_locations.push_back(mProgram.getAttribIndex("normal"));
    mMesh.set_attrib_locations(attrib_locations);
}

void
SceneBump::setup_low_polygon(const std::string &type)
{
    static const std::string vtx_shader_filename(GLMARK_DATA_PATH"/shaders/light-advanced.vert");
    static const std::string frg_shader_filename_std(GLMARK_DATA_PATH"/shaders/light-advanced.frag");
    static const std::string frg_shader_filename_normal(GLMARK_DATA_PATH"/shaders/light-advanced-normal-map.frag");
    Model model;

    if(!model.load_3ds(GLMARK_DATA_PATH"/models/asteroid-low.3ds"))
        return;

    model.calculate_normals();

    /* Tell the converter that we only care about position and normal attributes */
    std::vector<std::pair<Model::AttribType, int> > attribs;
    attribs.push_back(std::pair<Model::AttribType, int>(Model::AttribTypePosition, 3));
    attribs.push_back(std::pair<Model::AttribType, int>(Model::AttribTypeNormal, 3));
    if (type != "off")
        attribs.push_back(std::pair<Model::AttribType, int>(Model::AttribTypeTexcoord, 2));

    model.convert_to_mesh(mMesh, attribs);

    std::string frg_shader_filename;
    if (type == "off")
        frg_shader_filename = frg_shader_filename_std;
    else if (type == "normals")
        frg_shader_filename = frg_shader_filename_normal;

    if (!Scene::load_shaders_from_files(mProgram, vtx_shader_filename,
                                        frg_shader_filename))
    {
        return;
    }

    std::vector<GLint> attrib_locations;
    attrib_locations.push_back(mProgram.getAttribIndex("position"));
    attrib_locations.push_back(mProgram.getAttribIndex("normal"));
    if (type != "off")
        attrib_locations.push_back(mProgram.getAttribIndex("texcoord"));
    mMesh.set_attrib_locations(attrib_locations);

    if (type != "off") {
        Texture::load(GLMARK_DATA_PATH"/textures/asteroid-normal-map.png", &mTexture,
                      GL_NEAREST, GL_NEAREST, 0);
    }
}

void SceneBump::setup()
{
    Scene::setup();

    const std::string &bump_render = mOptions["bump-render"].value;

    if (bump_render == "off" || bump_render == "normals")
        setup_low_polygon(bump_render);
    else if (bump_render == "high-poly")
        setup_high_polygon();

    static const LibMatrix::vec4 lightPosition(20.0f, 20.0f, 10.0f, 1.0f);

    mMesh.build_vbo();

    mProgram.start();

    // Load lighting and material uniforms
    mProgram.loadUniformVector(lightPosition, "LightSourcePosition");

    // Calculate and load the half vector
    LibMatrix::vec3 halfVector(lightPosition.x(), lightPosition.y(), lightPosition.z());
    halfVector.normalize();
    halfVector += LibMatrix::vec3(0.0, 0.0, 1.0);
    halfVector.normalize();
    mProgram.loadUniformVector(halfVector, "LightSourceHalfVector");

    // Load texture sampler value
    mProgram.loadUniformScalar(0, "NormalMap");

    mCurrentFrame = 0;
    mRotation = 0.0;
    mRunning = true;
    mStartTime = Scene::get_timestamp_us() / 1000000.0;
    mLastUpdateTime = mStartTime;
}

void
SceneBump::teardown()
{
    mMesh.reset();

    mProgram.stop();
    mProgram.release();

    glDeleteTextures(1, &mTexture);

    Scene::teardown();
}

void SceneBump::update()
{
    double current_time = Scene::get_timestamp_us() / 1000000.0;
    double dt = current_time - mLastUpdateTime;
    double elapsed_time = current_time - mStartTime;

    mLastUpdateTime = current_time;

    if (elapsed_time >= mDuration) {
        mAverageFPS = mCurrentFrame / elapsed_time;
        mRunning = false;
    }

    mRotation += mRotationSpeed * dt;

    mCurrentFrame++;
}

void SceneBump::draw()
{
    LibMatrix::Stack4 model_view;

    // Load the ModelViewProjectionMatrix uniform in the shader
    LibMatrix::mat4 model_view_proj(mCanvas.projection());

    model_view.translate(0.0f, 0.0f, -3.5f);
    model_view.rotate(mRotation, 0.0f, 1.0f, 0.0f);
    model_view_proj *= model_view.getCurrent();

    mProgram.loadUniformMatrix(model_view_proj, "ModelViewProjectionMatrix");

    // Load the NormalMatrix uniform in the shader. The NormalMatrix is the
    // inverse transpose of the model view matrix.
    LibMatrix::mat4 normal_matrix(model_view.getCurrent());
    normal_matrix.inverse().transpose();
    mProgram.loadUniformMatrix(normal_matrix, "NormalMatrix");

    mMesh.render_vbo();
}

Scene::ValidationResult
SceneBump::validate()
{
    return Scene::ValidationUnknown;
}

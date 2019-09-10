/**
 * @file   VideoCommand.cc
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <iomanip>
#include <sstream>
#include <locale>
#include<algorithm>

#include "ImageCommand.h" // for enqueue_operations of Image type
#include "VideoCommand.h"
#include "VDMSConfig.h"
#include "defines.h"
//#include "VDMSClient.h" // for calling the client to store a video as a set of
//images, if necessary
#include "comm/Connection.h"
#include "protobuf/queryMessage.pb.h"

using namespace VDMS;

VideoCommand::VideoCommand(const std::string &cmd_name):
    RSCommand(cmd_name)
{
}

void VideoCommand::enqueue_operations(VCL::Video& video, const Json::Value& ops)
{
    // Correct operation type and parameters are guaranteed at this point
    for (auto& op : ops) {
        const std::string& type = get_value<std::string>(op, "type");
         std::string unit ;
        if (type == "threshold") {
            video.threshold(get_value<int>(op, "value"));

        }
        else if (type == "interval") {

            video.interval(
                VCL::Video::FRAMES,
                get_value<int>(op, "start"),
                get_value<int>(op, "stop"),
                get_value<int>(op, "step"));

        }
        else if (type == "resize") {
             video.resize(get_value<int>(op, "height"),
                          get_value<int>(op, "width") );

        }
        else if (type == "crop") {
            video.crop(VCL::Rectangle (
                        get_value<int>(op, "x"),
                        get_value<int>(op, "y"),
                        get_value<int>(op, "width"),
                        get_value<int>(op, "height") ));
        }
        else {
            throw ExceptionCommand(ImageError, "Operation not defined");
        }
    }
}

VCL::Video::Codec VideoCommand::string_to_codec(const std::string& codec)
{
    if (codec == "h263") {
        return VCL::Video::Codec::H263;
    }
    else if (codec == "xvid") {
        return VCL::Video::Codec::XVID;
    }
    else if (codec == "h264") {
        return VCL::Video::Codec::H264;
    }

    return VCL::Video::Codec::NOCODEC;
}

Json::Value VideoCommand::check_responses(Json::Value& responses)
{
    if (responses.size() != 1) {
        Json::Value return_error;
        return_error["status"]  = RSCommand::Error;
        return_error["info"] = "PMGD Response Bad Size";
        return return_error;
    }

    Json::Value& response = responses[0];

    if (response["status"] != 0) {
        response["status"]  = RSCommand::Error;
        // Uses PMGD info error.
        return response;
    }

    return response;
}

//========= AddVideo definitions =========

AddVideo::AddVideo() : VideoCommand("AddVideo")
{
    _storage_video = VDMSConfig::instance()->get_path_videos();
}

int AddVideo::construct_protobuf(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];

    int node_ref = get_value<int>(cmd, "_ref",
                                  query.get_available_reference());

    VCL::Video video((void*)blob.data(), blob.size());

    if (cmd.isMember("operations")) {
        enqueue_operations(video, cmd["operations"]);
    }

    // The container and codec are checked by the schema.
    // We default to mp4 and h264, if not specified
    const std::string& container =
                            get_value<std::string>(cmd, "container", "mp4");
    const std::string& file_name =
                            VCL::create_unique(_storage_video, container);

    // Modifiyng the existing properties that the user gives
    // is a good option to make the AddNode more simple.
    // This is not ideal since we are manupulating with user's
    // input, but for now it is an acceptable solution.
    Json::Value props = get_value<Json::Value>(cmd, "properties");
    props[VDMS_VID_PATH_PROP] = file_name;

    // Add Video node
    query.AddNode(node_ref, VDMS_VID_TAG, props, Json::Value());

    const std::string& codec = get_value<std::string>(cmd, "codec", "h264");
    VCL::Video::Codec vcl_codec = string_to_codec(codec);

    video.store(file_name, vcl_codec);

    // In case we need to cleanup the query
    error["video_added"] = file_name;

    if (cmd.isMember("link")) {
        add_link(query, cmd["link"], node_ref, VDMS_VID_EDGE);
    }

    return 0;
}

Json::Value AddVideo::construct_responses(
    Json::Value& response,
    const Json::Value& json,
    protobufs::queryMessage &query_res,
    const std::string& blob)
{
    Json::Value ret;
    ret[_cmd_name] = RSCommand::check_responses(response);

    return ret;
}

//========= AddVideo Bulk Loader definitions =========
AddVideoBL::AddVideoBL() : VideoCommand("AddVideoBL")
{
    _storage_video = VDMSConfig::instance()->get_path_videos();
}

std::string* split(std::string line, std::string delim){
    const char* ccArr = line.c_str();
    char cArr[line.size()+1];
    strcpy(cArr, ccArr);
    char del[delim.size()+1];
    strcpy(del, delim.c_str());
    std::string* result = new std::string[3];
    
    char* pch = strtok(cArr, del);
    int i = 0;
    while (pch != NULL){
        result[i] = pch;
        pch = strtok(NULL, del);
        //std::string testpch(pch);
        //std::cout << testpch.c_str() << std::endl;
        i++;
    }
    return result;
}

std::string exec2(const char* cmd) {
    std::cerr << cmd << std::endl;
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe){
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != NULL){
        //std::cout << "Buffer Data: " + buffer.data() << std::endl;
        result += buffer.data();
    }
    auto returnCode = pclose(pipe);
    std::cerr << returnCode << std::endl;
    //std::cerr << result << std::endl;
    return result;
}

int getTinSec(std::string snd){
    std::tm t = {};
    std::istringstream ss(snd);
    
    if (ss >> std::get_time(&t, "%H:%M:%S")){
        return t.tm_sec + t.tm_min * 60 + t.tm_hour * 60 * 60;
    }else{
        std::cerr << "Parse failed\n";
    }
    return 0;
}

void AddVideoBL::genClips(std::string fname, int csize){
    //Use ffprobe to find the duration:
    std::string p1 = "ffprobe ";
    std::string p2 = p1 + fname;
    std::string p3 = p2 + " 2>&1";
    const char* cmdstr = p3.c_str();
    std::string sOut = exec2(cmdstr);
    std::cerr << "exec2 complete" << std::endl;
    std::cerr << ("Output: " + sOut).c_str() << std::endl;
    //grep the output for duration
    std::istringstream f(sOut.c_str());
    std::string line;
    std::string dur;
    while (std::getline(f, line)){
        if (line.find("Duration") != std::string::npos){
            std::cerr << "Found it!" << std::endl;
            std::string* inf = split(line, ",");
            std::string fst = inf[0];
            fst.erase(std::remove_if(fst.begin(), fst.end(), ::isspace), fst.end());
            //"Duration:" has 9 characters
            std::string snd = fst.substr(9, fst.length()-10);
            //Duration should be a string in terms of hours:minutes:seconds
            //We want to convert it into a single integer which is seconds
            int seconds = getTinSec(snd);
            double nclips = std::ceil((double)seconds/(double)csize);
            int inclips = (int)nclips;
            int i = 0;
            for (i = 0; i < inclips; i++){
                int st = i * csize;
                std::string cmdstr2 = "ffmpeg -ss " + std::to_string(st)
                        + " -i " + fname + " -t " + std::to_string(csize)
                        + " -map_metadata 0" + " -c copy " + "-flags +global_header "
                        + "tmp" + std::to_string(i) + ".mp4";
                system(cmdstr2.c_str());
            }
            return;
        }
    }
    fprintf(stderr, "ERROR: Duration not found in ffprobe output\n");
    exit(-1);
}

int AddVideoBL::bulkLoader(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];

    std::ofstream lfile;
    lfile.open("fullfile.mp4", std::ofstream::binary);
    if (lfile.is_open()){
        lfile.write(blob.data(),blob.size());
        lfile.close();
    }
    int csize = get_value<int>(cmd, "clipSize", 2);
    genClips("fullfile.mp4", csize);
    //get the names of all files in current directory
    int i = 0;
    std::string fname = "tmp" + std::to_string(i) + ".mp4";
    //std::string fname = "tmp6.mp4";
    std::ifstream f(fname.c_str(), std::ifstream::binary);
    int lastRef = -1;
	
	Json::Value allprops = get_value<Json::Value>(cmd, "properties");
	
    while (f.good()){
        fprintf(stderr, "fname: %s\n", fname.c_str());
        fprintf(stderr, "In the while loop!");
        std::string line;
        std::string blob2 = "";
        if (f.is_open()){
            while (getline(f,line)){
                blob2 = blob2 + line + "\n";
            }
            f.close();
        }
        int node_ref = get_value<int>(cmd, "_ref",
                          query.get_available_reference());
        if (i == 0){
            lastRef = node_ref;
        }
        if (lastRef >= node_ref){//just give clips ascending ref numbers
            //in the case where they all have the same ref number
            lastRef++;
            node_ref = lastRef;
        }
        VCL::Video video((void*)blob2.data(), blob2.size());

        if (cmd.isMember("operations")) {
            enqueue_operations(video, cmd["operations"]);
        }

        // The container and codec are checked by the schema.
        // We default to mp4 and h264, if not specified
        const std::string& container =
                            get_value<std::string>(cmd, "container", "mp4");

        const std::string& file_name =
                            VCL::create_unique(_storage_video, container);
            

        // Modifiyng the existing properties that the user gives
        // is a good option to make the AddNode more simple.
        // This is not ideal since we are manupulating with user's
        // input, but for now it is an acceptable solution.
        Json::Value props = allprops[i];
		
        props[VDMS_VID_PATH_PROP] = file_name;

        // Add Video node
        query.AddNode(node_ref, VDMS_VID_TAG, props, Json::Value());
            

        const std::string& codec = get_value<std::string>(cmd, "codec", "h264");
            
        VCL::Video::Codec vcl_codec = string_to_codec(codec);

        try {
            video.store(file_name, vcl_codec);
            fprintf(stderr, "Stored video with right codec: %s\n", fname.c_str());
        } catch(const std::exception& e) {
            fprintf(stderr, "Failed to store video: %s\n", fname.c_str());
            std::cerr << e.what() << "\n";
        } catch(const VCL::Exception& e){
            fprintf(stderr, "Failed to store video: %s\n", fname.c_str());
            std::cerr << e.name << "\n";
        }

        // In case we need to cleanup the query
        error["video_added"] = file_name;

        if (cmd.isMember("link")) {
            add_link(query, cmd["link"], node_ref, VDMS_VID_EDGE);
        }
        i++;
        fname = "tmp" + std::to_string(i) + ".mp4";
        f.open(fname);
    }
    return 0;
        
}

const std::string AddVideoBL::query(const std::string &json, const std::vector<std::string *> blobs){
    std::string addr = "localhost";
    const int port = 55555;
    comm::ConnClient _conn(addr,port);
    protobufs::queryMessage cmd;
    cmd.set_json(json);

    for (auto& it : blobs) {
        std::string *blob = cmd.add_blobs();
        *blob = *it;
    }

    std::basic_string<uint8_t> msg(cmd.ByteSize(),0);
    cmd.SerializeToArray((void*)msg.data(), msg.length());
    _conn.send_message(msg.data(), msg.length());

    // Wait now for response
    // TODO: Perhaps add an asynchronous version too.
    msg = _conn.recv_message();
    protobufs::queryMessage resp;
    resp.ParseFromArray((const void*)msg.data(), msg.length());

    return resp.json();
}

int AddVideoBL::storeNthFrames(const std::string& blob, int n, const std::string& vname)
{
	
	std::ofstream lfile;
    lfile.open("fullfile.mp4", std::ofstream::binary);
    if (lfile.is_open()){
        lfile.write(blob.data(),blob.size());
        lfile.close();
    }
    fprintf(stderr, "%s\n", vname.c_str());
    std::string cmdstr = "./skipnth.sh fullfile.mp4 " + std::to_string(n);
    system(cmdstr.c_str());
    //Use client script to add generated images to the database
    //std::string clscript = "python addAllImgs.py " + vname;
    //NOTE: This system() call is temporary until we figure out how to invoke AddImage functions to load images
    //directly
    //system(clscript.c_str());
    std::string jsoncmd;
    std::vector<std::string *> imgblobs;
    //Note: the json string has to reflect that there could be multiple queries
    //to execute in batch, just like with the python client code. Therefore,
    //we put array brackets around the queries, which are introduced multiple times.
    jsoncmd = "";
    int numBlobs = 0;
    int i = 0;
    for (i = 0; i < 10000; i++){ //all 4-digit numbers
        std::string old_string = std::to_string(i);
        std::string new_string = std::string(4 - old_string.length(), '0') + old_string;
        std::string fname = "img_" + new_string + ".png";
        std::ifstream tmpfile;
        //tmpfile.open(fname.c_str(), std::ios::binary);
        tmpfile.open(fname.c_str(), std::ifstream::binary);
        if (tmpfile.is_open()){
            //std::string blob2 = "";
            //std::string line;
//            while (getline(tmpfile, line)){
//                blob2 = blob2 + line + "\n";
//            }
//            std::stringstream buffer;
//            buffer << tmpfile.rdbuf();
//            blob2 = buffer.str();
            //next thing to try: just ordinary read() function, then convert
            //char* into string.
            //get length of file first:
            tmpfile.seekg(0,tmpfile.end);
            int flen = tmpfile.tellg();
            tmpfile.seekg(0,tmpfile.beg);
            char * buffer = new char[flen];
            std::cerr << "Reading " << flen << " characters... ";
            tmpfile.read(buffer,flen);
            std::string blob2(buffer);
            imgblobs.push_back(&blob2);
            tmpfile.close();
            std::string fcmd = "{\"AddImage\":{\"format\":\"png\",\"properties\":{\"name\":\"Video Image" + std::to_string(numBlobs) + "\",\"vidname\":\"" + vname + "\"}}}";
            if (numBlobs == 0){
                jsoncmd = jsoncmd + fcmd;
            }else {
                jsoncmd = jsoncmd + "," + fcmd;
            }
            
            numBlobs++;
        }
        
    }
    std::string testcmd = "{\"AddImage\":{\"format\":\"png\",\"properties\":{\"name\":\"Video Image\",\"vidname\":\"" + vname + "\"}}}";
    jsoncmd = "[" + jsoncmd + "]";
    testcmd = "[" + testcmd + "]";
    const std::string jcmd = jsoncmd;
    const std::string tcmd = testcmd;
    //std::cout << jsoncmd.c_str() << std::endl;
    const std::vector<std::string *> imgs = imgblobs;
    std::vector<std::string *> testImg;
    testImg.push_back(imgblobs.at(0));
    const std::vector<std::string *> timg = testImg;
    //const std::string resp = query(jcmd, imgs);
    const std::string resp = query(tcmd, timg);
    std::cerr << resp.c_str() << std::endl;
    return 0;
	
}

int AddVideoBL::construct_protobuf(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];
    const int accessTime = 
                            get_value<int>(cmd, "accessTime", 1);
    const int storeSize = 
                            get_value<int>(cmd, "storeSize", 1);
    //we will treat accessTime and storeSize as override variables: if either
    //is 1, we will override the encoding choice.
    if (accessTime == 1 && storeSize == 1){ //if both 1, just load as normal
	//That is, assume the user might want to use frameskipping on the whole video and just load
	//images, so retrieval of the images for NN evaluation might be easier.

        int node_ref = get_value<int>(cmd, "_ref",
                                      query.get_available_reference());

        VCL::Video video((void*)blob.data(), blob.size());

        if (cmd.isMember("operations")) {
            enqueue_operations(video, cmd["operations"]);
        }

        // The container and codec are checked by the schema.
        // We default to mp4 and h264, if not specified
        const std::string& container =
                                get_value<std::string>(cmd, "container", "mp4");
		
		const int skipnth = get_value<int>(cmd, "frameSkip", 0);
		fprintf(stderr, "skipnth: %d\n", skipnth);
		Json::Value props = get_value<Json::Value>(cmd, "properties");
		const std::string vidname = get_value<std::string>(props, "vidname", "");
		fprintf(stderr, "vidname: %s\n", vidname.c_str());
		if (skipnth > 0 && vidname != ""){
			return AddVideoBL::storeNthFrames(blob, skipnth, vidname);
		}
    
        const std::string& file_name =
                            VCL::create_unique(_storage_video, container);

        // Modifiyng the existing properties that the user gives
        // is a good option to make the AddNode more simple.
        // This is not ideal since we are manupulating with user's
        // input, but for now it is an acceptable solution.
        props[VDMS_VID_PATH_PROP] = file_name;

        // Add Video node
        query.AddNode(node_ref, VDMS_VID_TAG, props, Json::Value());

        const std::string& codec = get_value<std::string>(cmd, "codec", "h264");
        VCL::Video::Codec vcl_codec = string_to_codec(codec);

        video.store(file_name, vcl_codec);

        // In case we need to cleanup the query
        error["video_added"] = file_name;

        if (cmd.isMember("link")) {
            add_link(query, cmd["link"], node_ref, VDMS_VID_EDGE);
        }
    }else { 
        int retval = AddVideoBL::bulkLoader(query, jsoncmd, blob, grp_id, error);
    }
    

    return 0;
}

Json::Value AddVideoBL::construct_responses(
    Json::Value& response,
    const Json::Value& json,
    protobufs::queryMessage &query_res,
    const std::string& blob)
{
    Json::Value ret;
    ret[_cmd_name] = RSCommand::check_responses(response);

    return ret;
}

//========= UpdateVideo definitions =========

UpdateVideo::UpdateVideo() : VideoCommand("UpdateVideo")
{
}

int UpdateVideo::construct_protobuf(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];

    int node_ref = get_value<int>(cmd, "_ref", -1);

    Json::Value constraints = get_value<Json::Value>(cmd, "constraints");

    Json::Value props = get_value<Json::Value>(cmd, "properties");

    Json::Value remove_props = get_value<Json::Value>(cmd, "remove_props");

    // Update Image node
    query.UpdateNode(node_ref, VDMS_VID_TAG, props,
                        remove_props,
                        constraints,
                        get_value<bool>(cmd, "unique", false));

    return 0;
}

Json::Value UpdateVideo::construct_responses(
    Json::Value& responses,
    const Json::Value& json,
    protobufs::queryMessage &query_res,
    const std::string &blob)
{
    assert(responses.size() == 1);

    Json::Value ret;

    // TODO In order to support "codec" or "operations", we could
    // implement VCL save operation here.

    ret[_cmd_name].swap(responses[0]);
    return ret;
}

//========= FindVideo definitions =========

FindVideo::FindVideo() : VideoCommand("FindVideo")
{
}

int FindVideo::construct_protobuf(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];

    Json::Value results = get_value<Json::Value>(cmd, "results");

    // Unless otherwhise specified, we return the blob.
    if (get_value<bool>(results, "blob", true)){
        results["list"].append(VDMS_VID_PATH_PROP);
    }

    query.QueryNode(
            get_value<int>(cmd, "_ref", -1),
            VDMS_VID_TAG,
            cmd["link"],
            cmd["constraints"],
            results,
            get_value<bool>(cmd, "unique", false)
            );

    return 0;
}

Json::Value FindVideo::construct_responses(
    Json::Value& responses,
    const Json::Value& json,
    protobufs::queryMessage &query_res,
    const std::string &blob)
{
    const Json::Value& cmd = json[_cmd_name];

    Json::Value ret;

    auto error = [&](Json::Value& res)
    {
        ret[_cmd_name] = res;
        return ret;
    };

    Json::Value resp = check_responses(responses);
    if (resp["status"] != RSCommand::Success) {
        return error(resp);
    }

    Json::Value& FindVideo = responses[0];

    bool flag_empty = true;

    for (auto& ent : FindVideo["entities"]) {

        if(!ent.isMember(VDMS_VID_PATH_PROP)){
            continue;
        }

        std::string video_path = ent[VDMS_VID_PATH_PROP].asString();
        ent.removeMember(VDMS_VID_PATH_PROP);

        if (ent.getMemberNames().size() > 0) {
            flag_empty = false;
        }
        try {
            if (!cmd.isMember("operations") &&
                !cmd.isMember("container")  &&
                !cmd.isMember("codec"))
            {
                // Return video as is.
                std::ifstream ifile(video_path, std::ifstream::in);
                ifile.seekg(0, std::ios::end);
                size_t encoded_size = (long)ifile.tellg();
                ifile.seekg(0, std::ios::beg);

                std::string* video_str = query_res.add_blobs();
                video_str->resize(encoded_size);
                ifile.read((char*)(video_str->data()), encoded_size);
                ifile.close();
            }
            else {

                VCL::Video video(video_path);

                if (cmd.isMember("operations")) {
                    enqueue_operations(video, cmd["operations"]);
                }

                const std::string& container =
                            get_value<std::string>(cmd, "container", "mp4");
                const std::string& file_name =
                            VCL::create_unique("/tmp/", container);
                const std::string& codec =
                            get_value<std::string>(cmd, "codec", "h264");

                VCL::Video::Codec vcl_codec = string_to_codec(codec);
                video.store(file_name, vcl_codec); // to /tmp/ for encoding.

                auto video_enc = video.get_encoded();
                int size = video_enc.size();

                if (size > 0) {

                    std::string* video_str = query_res.add_blobs();
                    video_str->resize(size);
                    std::memcpy((void*)video_str->data(),
                                (void*)video_enc.data(),
                                size);
                }
                else {
                    Json::Value return_error;
                    return_error["status"]  = RSCommand::Error;
                    return_error["info"] = "Video Data not found";
                    error(return_error);
                }
            }
        } catch (VCL::Exception e) {
            print_exception(e);
            Json::Value return_error;
            return_error["status"]  = RSCommand::Error;
            return_error["info"] = "VCL Exception";
            return error(return_error);
        }
    }

    if (flag_empty) {
        FindVideo.removeMember("entities");
    }

    ret[_cmd_name].swap(FindVideo);
    return ret;
}

//========= FindFrames definitions =========

FindFrames::FindFrames() : VideoCommand("FindFrames")
{
}

bool FindFrames::get_interval_index (const Json::Value& cmd,
                                     Json::ArrayIndex& op_index)
{
    if (cmd.isMember("operations")) {
        const auto operations = cmd["operations"];
        for (auto i = 0; i < operations.size(); i++) {
            const auto op = operations[i];
            const std::string& type = get_value<std::string>(op, "type");
            if (type == "interval") {
                op_index = i;
                return true;
            }
        }
    }
    return false;
}

int FindFrames::construct_protobuf(
    PMGDQuery& query,
    const Json::Value& jsoncmd,
    const std::string& blob,
    int grp_id,
    Json::Value& error)
{
    const Json::Value& cmd = jsoncmd[_cmd_name];

    // We try to catch the missing attribute error before
    // initiating a PMGD query
    Json::ArrayIndex tmp;
    bool is_interval = get_interval_index(cmd, tmp);
    bool is_frames   = cmd.isMember("frames");

    if (!(is_frames != is_interval)) {
        error["status"]  = RSCommand::Error;
        error["info"] = "Either one of 'frames' or 'operations::interval' "
                        "must be specified";
        return -1;
    }

    Json::Value results = get_value<Json::Value>(cmd, "results");
    results["list"].append(VDMS_VID_PATH_PROP);

    query.QueryNode(
            get_value<int>(cmd, "_ref", -1),
            VDMS_VID_TAG,
            cmd["link"],
            cmd["constraints"],
            results,
            get_value<bool>(cmd, "unique", false)
            );

    return 0;
}

Json::Value FindFrames::construct_responses(
    Json::Value& responses,
    const Json::Value& json,
    protobufs::queryMessage &query_res,
    const std::string &blob)
{
    const Json::Value& cmd = json[_cmd_name];

    Json::Value ret;

    auto error = [&](Json::Value& res)
    {
        ret[_cmd_name] = res;
        return ret;
    };

    Json::Value resp = check_responses(responses);
    if (resp["status"] != RSCommand::Success) {
        return error(resp);
    }

    Json::Value& FindFrames = responses[0];

    bool flag_empty = true;

    for (auto& ent : FindFrames["entities"]) {

        std::string video_path = ent[VDMS_VID_PATH_PROP].asString();
        ent.removeMember(VDMS_VID_PATH_PROP);

        if (ent.getMemberNames().size() > 0) {
            flag_empty = false;
        }

        try {
            std::vector<unsigned> frames;

            // Copy of operations is needed, as we pass the operations to
            // the enqueue_operations() method of ImageCommands class, and
            // it should not include 'interval' operation.
            Json::Value operations  = cmd["operations"];

            Json::ArrayIndex interval_idx;
            bool is_interval = get_interval_index(cmd, interval_idx);
            bool is_frames   = cmd.isMember("frames");

            if (is_frames) {
                for (auto& fr : cmd["frames"]) {
                    frames.push_back(fr.asUInt());
                }
            }
            else if (is_interval) {

                Json::Value interval_op = operations[interval_idx];

                int start = get_value<int>(interval_op, "start");
                int stop  = get_value<int>(interval_op, "stop");
                int step  = get_value<int>(interval_op, "step");

                for (int i = start; i < stop; i += step)  {
                    frames.push_back(i);
                }

                Json::Value deleted;
                operations.removeIndex(interval_idx, &deleted);
            }
            else {
                // This should never happen, as we check this condition in
                // FindFrames::construct_protobuf(). In case this happens, it
                // is better to signal it rather than to continue
                Json::Value return_error;
                return_error["status"]  = RSCommand::Error;
                return_error["info"] = "No 'frames' or 'interval' parameter";
                return error(return_error);
            }

            VCL::Video video(video_path);

            // By default, return frames as PNGs
            VCL::Image::Format format = VCL::Image::Format::PNG;

            FindImage img_cmd;

            if (cmd.isMember("format")) {

                format = img_cmd.get_requested_format(cmd);

                if (format == VCL::Image::Format::NONE_IMAGE ||
                    format == VCL::Image::Format::TDB) {
                    Json::Value return_error;
                    return_error["status"] = RSCommand::Error;
                    return_error["info"]   = "Invalid Return Format for FindFrames";
                    return error(return_error);
                }
            }

            for (auto idx : frames) {
                cv::Mat mat = video.get_frame(idx);
                VCL::Image img(mat, false);
                if (!operations.empty()) {
                    img_cmd.enqueue_operations(img, operations);
                }

                std::vector<unsigned char> img_enc;
                img_enc = img.get_encoded_image(format);

                if (!img_enc.empty()) {
                    std::string* img_str = query_res.add_blobs();
                    img_str->resize(img_enc.size());
                    std::memcpy((void*)img_str->data(),
                                (void*)img_enc.data(),
                                img_enc.size());
                }
                else {
                    Json::Value return_error;
                    return_error["status"] = RSCommand::Error;
                    return_error["info"]   = "Image Data not found";
                    return error(return_error);
                }
            }
        }

        catch (VCL::Exception e) {
            print_exception(e);
            Json::Value return_error;
            return_error["status"]  = RSCommand::Error;
            return_error["info"] = "VCL Exception";
            return error(return_error);
        }
    }

    if (flag_empty) {
        FindFrames.removeMember("entities");
    }

    ret[_cmd_name].swap(FindFrames);
    return ret;
}

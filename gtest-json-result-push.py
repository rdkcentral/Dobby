#!/usr/bin/python3

####################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
####################################################################################


# This utility depends on the xmltodict, json and requests python library

import sys
import xmltodict
import json
import requests
import os


from requests.auth import HTTPBasicAuth

# function to read json file as input and return minified json data as string
def json_string(xml_file_path, git_folder_path):

    # Change current working directory to the script directory
    os.chdir(git_folder_path)
    commit_id = os.popen('git log --pretty="format:%H" -n 1').read()
    component_name=os.popen('git remote -v | grep fetch | cut -d "/" -f2 | cut -d "." -f1').read()
    # TBD : Need to get the execution link from the environment variable
    execution_link="https://ci.comcast.net"
    developer=os.popen('git log --pretty="format:%an" -n 1').read()
    jira_ticket=os.popen('git log --pretty="format:%s" -n 1 | cut -d ":" -f1').read()

    json_root = {
        "commit_id": commit_id.strip(),
        "component_name": component_name.strip(),
        "execution_link": execution_link.strip(),
        "developer": developer.strip(),
        "jira_ticket": jira_ticket.strip()
    }

    with open(xml_file_path) as json_file:
        # minify the json message
        json_data = json.dumps(json.loads(json_file.read()))
        json_root["test_cases_results"] = json.loads(json_data)

        
    return json.dumps(json_root)

# function to http post json data to a url
def post_json(json_data, url):


    # Get username and password from env variable
    username = os.environ.get('AUTOMATICS_UNAME')
    password = os.environ.get('AUTOMATICS_PWD')
    passcode = os.environ.get('AUTOMATICS_PASSCODE')
    passvalue = "Basic " + passcode 
    headers = {
        'Content-type': 'application/json',
        'Authorization': passvalue
               
    }

    # print("Automatics simple auth uname : " + username)
    # print("Automatics simple auth uname : " + password)
    # print("Automatics simple auth passcode : " + passcode)
    
    # response = requests.post(url, data=json_data, headers=headers, auth=HTTPBasicAuth(username, password))
    response = requests.post(url, data=json_data, headers=headers)
    print(response)

# Loop through all the xml files in the folder and call the xml_to_json function
# to convert xml to json and then call the post_json function to post the json data to a url
def post_json_to_url(json_folder_path, url, git_folder_path):

    try :

        for filename in os.listdir(json_folder_path):
            if filename.endswith(".json"):
                json_file_path = os.path.join(json_folder_path, filename)
                json_data = json_string(json_file_path, git_folder_path)
                print(json_data)
                post_json(json_data, url)
   
    except FileNotFoundError as e:
        print(f"Test results are not found in the specified {git_folder_path} folder.")
        print(f"Please check the folder path and try again. Error: {e}")

def main():
    
        # Check if the script is called with the correct number of arguments
        if len(sys.argv) != 4:
            print("Usage: python3 gtest-xml-json-coverter.py <gtest-xml-folder-path> <upload url> <git-local-repo-path>")
            sys.exit(1)

        
        print("Converting xml to json")

        print("Siva: Prinitng sys.argv[1]")
        print(sys.argv[1])

        print("Siva: Prinitng sys.argv[2]")
        print(sys.argv[2])

        print("Siva: Prinitng sys.argv[3]")
        print(sys.argv[3])
        post_json_to_url(sys.argv[1], sys.argv[2], sys.argv[3])

if __name__ == "__main__":
    main()

import os
import sys
import json
import vdms
import re

db = vdms.vdms()
db.connect("localhost")
blob_arr = []
def ithquery(fname, vname):
	fd = open(fname)
	blob = fd.read()

	addImage = {}
	props = {}
	props["name"] = "Video Image: " + fname
	props["vidname"] = vname
	props["length"] = "N/A"

	addImage["properties"] = props

	query = {}
	query["AddImage"] = addImage
	fd.close()
	return blob,query
	
dirs = os.listdir('.')
vname = sys.argv[1]
all_queries = []
for file in dirs:
	pattern = re.compile('img_[0-9][0-9][0-9][0-9].png')
	if pattern.match(file):
		blob, query = ithquery(file, vname)
		all_queries.append(query)
		blob_arr.append(blob)
response, res_arr = db.query(all_queries,[blob_arr])
print(response) 
for r in response:
  if r[u'AddImage'][u'status'] != 0:
    print("Error in response!")
#8/6/2019: Check this script and the VideoCommand.cc script tomorrow and make sure
#that they are actually compatible. 
db.disconnect()
		
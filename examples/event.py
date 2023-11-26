# event.py
import build.event_pb2 as E
from google.protobuf.json_format import MessageToJson

event = E.Event(id=E.Type.CREATED, name='test', description='created event!')

def print_section(title, content):
    print(f"{title}:\n{'-'*len(title)}\n{content}\n")

print_section("Python structure", event)
print_section("JSON structure", MessageToJson(event))
print_section("Serialized structure", event.SerializeToString())

# This is an import of generated code, stripped for usability here.
#
# @partially-generated



class CounterUnit(object):
    BYTES = 0
    SECONDS = 1
    ITEMS = 2
    RATIO = 3

    _VALUES_TO_NAMES = {
        0: "BYTES",
        1: "SECONDS",
        2: "ITEMS",
        3: "RATIO",
    }

    _NAMES_TO_VALUES = {
        "BYTES": 0,
        "SECONDS": 1,
        "ITEMS": 2,
        "RATIO": 3,
    }


class StackTrace(object):
    """
  Attributes:
   - frames
  """

    __init__ = None


class StackFrame(object):
    """
  Attributes:
   - identifier
   - symbol
   - codeLocation
   - lineNumber
  """

    __init__ = None


class PathDefinition(object):
    """
  Attributes:
   - begin
   - end
   - name
  """

    __init__ = None


class CriticalPathData(object):
    """
  Attributes:
   - points
   - totalCost
   - name
  """

    __init__ = None


class Event(object):
    """
  Attributes:
   - traceID
   - type
   - id1
   - id2
   - timestamp
   - annotations
   - sequenceNumber
  """

    __init__ = None


class Properties(object):
    """
  Attributes:
   - coreProps
   - customProps
   - counterProps
   - errors
   - sets
   - stacktraces
   - stacktraceHashes
   - stacktraceHashResolvers
  """

    __init__ = None


class Point(object):
    """
  Attributes:
   - id
   - timestamp: Timestamps are in units of microseconds.  This is the timestamp after clocks have been aligned
   - unalignedTimestamp: Timestamps are in units of microseconds.  This is the original timestamp before any correction has been applied
   - properties
   - sequenceNumber
  """

    __init__ = None


class Block(object):
    """
  Attributes:
   - id
   - begin
   - end
   - otherPoints
   - properties
  """

    __init__ = None


class Edge(object):
    """
  Edges express causal relationship between points: sourcePoint being the cause and targetPoint the effect.

  Attributes:
   - sourcePoint
   - targetPoint
   - properties
  """

    __init__ = None


class Trace(object):
    """
  IDs for a single object type are unique within a trace.

  Attributes:
   - id
   - executionUnits
   - blocks
   - points
   - version
   - edges
   - properties
  """

    __init__ = None


class ExecutionUnit(object):
    """
  Timestamps within a single Execution Unit are considered to be in sync.

  Attributes:
   - id
   - blocks
   - properties
  """

    __init__ = None


def StackTrace__init__(
    self,
    frames=None,
):
    self.frames = frames


StackTrace.__init__ = StackTrace__init__


def StackFrame__init__(
    self,
    identifier=None,
    symbol=None,
    codeLocation=None,
    lineNumber=None,
):
    self.identifier = identifier
    self.symbol = symbol
    self.codeLocation = codeLocation
    self.lineNumber = lineNumber


StackFrame.__init__ = StackFrame__init__


def PathDefinition__init__(
    self,
    begin=None,
    end=None,
    name=None,
):
    self.begin = begin
    self.end = end
    self.name = name


PathDefinition.__init__ = PathDefinition__init__


def CriticalPathData__init__(
    self,
    points=None,
    totalCost=None,
    name=None,
):
    self.points = points
    self.totalCost = totalCost
    self.name = name


CriticalPathData.__init__ = CriticalPathData__init__


def Event__init__(
    self,
    traceID=None,
    type=None,
    id1=None,
    id2=None,
    timestamp=None,
    annotations=None,
    sequenceNumber=None,
):
    self.traceID = traceID
    self.type = type
    self.id1 = id1
    self.id2 = id2
    self.timestamp = timestamp
    self.annotations = annotations
    self.sequenceNumber = sequenceNumber


Event.__init__ = Event__init__


def Properties__init__(
    self,
    coreProps=None,
    customProps=None,
    counterProps=None,
    errors=None,
    sets=None,
    stacktraces=None,
    stacktraceHashes=None,
    stacktraceHashResolvers=None,
):
    self.coreProps = coreProps
    self.customProps = customProps
    self.counterProps = counterProps
    self.errors = errors
    self.sets = sets
    self.stacktraces = stacktraces
    self.stacktraceHashes = stacktraceHashes
    self.stacktraceHashResolvers = stacktraceHashResolvers


Properties.__init__ = Properties__init__


def Block__init__(
    self,
    id=None,
    begin=None,
    end=None,
    otherPoints=None,
    properties=None,
):
    self.id = id
    self.begin = begin
    self.end = end
    self.otherPoints = otherPoints
    self.properties = properties


Block.__init__ = Block__init__


def Point__init__(
    self,
    id=None,
    timestamp=None,
    unalignedTimestamp=None,
    properties=None,
    sequenceNumber=None,
):
    self.id = id
    self.timestamp = timestamp
    self.unalignedTimestamp = unalignedTimestamp
    self.properties = properties
    self.sequenceNumber = sequenceNumber


Point.__init__ = Point__init__


def Edge__init__(
    self,
    sourcePoint=None,
    targetPoint=None,
    properties=None,
):
    self.sourcePoint = sourcePoint
    self.targetPoint = targetPoint
    self.properties = properties


Edge.__init__ = Edge__init__


def Trace__init__(
    self,
    id=None,
    executionUnits=None,
    blocks=None,
    points=None,
    version=None,
    edges=None,
    properties=None,
):
    self.id = id
    self.executionUnits = executionUnits
    self.blocks = blocks
    self.points = points
    self.version = version
    self.edges = edges
    self.properties = properties


Trace.__init__ = Trace__init__


def ExecutionUnit__init__(
    self,
    id=None,
    blocks=None,
    properties=None,
):
    self.id = id
    self.blocks = blocks
    self.properties = properties


ExecutionUnit.__init__ = ExecutionUnit__init__

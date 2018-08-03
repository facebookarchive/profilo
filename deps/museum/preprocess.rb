# Copyright 2004-present Facebook. All Rights Reserved.

# This script munges the model data and adds in some computable data that mustache is not able to:
# 1) Method parameters are converted from a string array of types to an object array, where the
#    object has a type and an idx field, with the idx field incrementing across each parameter list:
#
#    [ "int", "char", "bool" ]
#    becomes
#    [ { type: "int", idx: 1 }, { type: "char", idx: 2}, { type: "bool", idx: 3 } ]
#
#    This allows us to avoid redundantly specifying parameter names in the model, but still generate
#    sane parameter lists like 'void foo(int p1, char p2, bool p3)'.
# 2) A hash of the model file is added to the output so that the generated code can quickly tell if
#    it's out of date relative to the model.

require 'digest'
require 'yaml'
require 'json'

modelname = ARGV[0]
rawmodel = File.read(modelname)
data = YAML.load(rawmodel)

(data['namespaces'] or []).each do |namespace|
  (namespace['classes'] or []).each do |classSymbols|
    (classSymbols['symbols'] or []).each do |symbol|
      if symbol['params'] != nil
        newParams = []
        idx = 1
        symbol['params'].each do |paramType|
          newParams << { "type" => paramType, "idx" => idx }
          idx += 1
        end
        symbol['params'] = newParams
      end
      symbol['returnsSomething'] = (symbol['returnType'] != nil && symbol['returnType'] != 'void')
    end
  end
  if namespace['nsName'] != nil
    namespace['hasNsName'] = true
    nsObjs = []
    namespace['nsName'].each do |nsName|
      s = nsName.split
      if s.count > 1
        nsObjs << { "name" => s[1], "inline" => true }
      else
        nsObjs << { "name" => s[0] }
      end
    end
    namespace['nsName'] = nsObjs
  end
end

data['modelHash'] = Digest::MD5.hexdigest(rawmodel)

puts JSON.dump(data)

/* Invoke from Toybox with: js ARGS.JS hello 123 */
print("scriptPath:", scriptPath);
for (var i = 0; i < scriptArgs.length; i++)
    print("arg[" + i + "] =", scriptArgs[i]);

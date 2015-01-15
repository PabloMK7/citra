var util = require('util');
var exec = require('child_process').exec;

var email = 'chin.bimbo@gmail.com';
var password = process.env.MEGA_PASSWORD;
var sourceFileName = 'build.7z';
var dstFileName = process.env.APPVEYOR_REPO_COMMIT.substring(0, 8) + " - " + 
                process.env.APPVEYOR_REPO_COMMIT_MESSAGE.substring(0, 100) + ".7z";

var cmd = util.format('megaput ../%s --path \"/Root/Citra/Windows/%s\" --username=%s --password=%s --no-progress',
                        sourceFileName,
                        dstFileName,
                        email,
                        password);

if (process.env.APPVEYOR_REPO_BRANCH == "master") {
    console.log("Uploading file " + dstFileName + " to Mega...");
    exec(cmd, function(error, stdout, stderr) {
        console.log('stdout: ' + stdout);
        console.log('stderr: ' + stderr);
        if (error !== null) {
            console.log('exec error: ' + error);
        }
    });            
}

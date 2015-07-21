var app = (function(){
    function onload() {
        var z = zcm.create()
        z.subscribe('EXAMPLE', function(channel, msg) {
            console.log('Got EXAMPLE: ', msg);
            z.publish('FOOBAR', msg);
        });
    }
    return {
        onload: onload,
    };
})();

onload = app.onload;

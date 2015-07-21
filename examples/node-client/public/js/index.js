var app = (function(){
    function onload() {
        var z = zcm.create()
        setInterval(function(){
            z.publish('FOOBAR', {g:9, f:2});
        }, 100);
        z.subscribe('EXAMPLE', function(channel, msg) {
            console.log('Got EXAMPLE: ', msg);
        });
    }
    return {
        onload: onload,
    };
})();

onload = app.onload;

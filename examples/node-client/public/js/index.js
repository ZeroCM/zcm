onload = function(){
    var z = zcm.create()
    z.subscribe('EXAMPLE', function(channel, msg) {
        console.log('Got EXAMPLE: ', msg);
        z.publish('FOOBAR', msg);
    });
}

var app = (function(){

     var zc = zcomm.create();

    $('form').submit(function(e){
        zc.publish('foobar', $('#m').val());
        $('#m').val('');
        return false;
    });

    zc.subscribe('SOME_CHANNEL', function(channel, data){
        var t = 'channel='+channel+', data='+data;
        $('#messages').append($('<li>').text(t));
    });

    return { onload: function(){} };
})();

window.onload = app.onload;

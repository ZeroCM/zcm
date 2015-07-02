var app = (function(){

     var zc = zcomm.create();

    $('form').submit(function(e){
        zc.publish('PUB_CHANNEL', $('#m').val());
        $('#m').val('');
        return false;
    });

    zc.subscribe('SUB_CHANNEL', function(channel, data){
        var t = 'channel='+channel+', data='+data;
        $('#messages').append($('<li>').text(t));
    });

    return { onload: function(){} };
})();

window.onload = app.onload;

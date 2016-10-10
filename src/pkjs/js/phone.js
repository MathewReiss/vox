Pebble.addEventListener("ready", function(e){
	//Pebble.sendAppMessage({"READY" : ""});
});

Pebble.addEventListener("appmessage", function(e){
	var request = JSON.parse(JSON.stringify(e.payload));
	
	var lang1 = request.LANG_1;
	var lang2 = request.LANG_2;

            /*                                    API req (+)        replace !                  replace ,                  replace ?                   replace .                    unencode &           unencode =           */
	var raw = encodeURIComponent(request.RAW_TEXT.replace(/ /g, '+').replace(/!\+/g, '!&text=').replace(/,\+/g, ',&text=').replace(/\?\+/g, '?&text=').replace(/\.\+/g, '.&text=')).replace(/%26/g, '&').replace(/%3D/g, '='); 
	
	if(lang1 === "DT"){
		HTTP_GET("https://translate.yandex.net/api/v1.5/tr.json/detect?key=" + "trnsl.1.1.20150628T022121Z.3e4ed12e67856955.1783499c447e0e84a5835f645bdcf4f3165d5418" + "&text=" + raw, lang1, lang2, raw);
	}
	else{
		HTTP_GET("https://translate.yandex.net/api/v1.5/tr.json/translate?key=" + "trnsl.1.1.20150628T022121Z.3e4ed12e67856955.1783499c447e0e84a5835f645bdcf4f3165d5418" + "&lang=" + lang1 + "-" + lang2 + "&text=" + raw, lang1, lang2, raw);
	}
});

function HTTP_GET(url, lang1, lang2, raw){
	//console.log("URL: " + url);
	try{
		var request = new XMLHttpRequest();
		request.onreadystatechange = function(){
			if(request.readyState === 4){
				if(request.status === 200){
					var response = JSON.parse(request.responseText);
					if(lang1 === "DT"){
						lang1 = response.lang;
						Pebble.sendAppMessage({"LANG_1" : lang1});
						HTTP_GET("https://translate.yandex.net/api/v1.5/tr.json/translate?key=" + "trnsl.1.1.20150628T022121Z.3e4ed12e67856955.1783499c447e0e84a5835f645bdcf4f3165d5418" + "&lang=" + lang1 + "-" + lang2 + "&text=" + raw, lang1, lang2, raw);
					}
					else{
						var message = response.text.join(' ').replace(/\+/g, ' ');
						if(message.length > 200) message = message.substring(0, 197)+'...';
						Pebble.sendAppMessage({"TRANSLATED_TEXT" : message});
            console.log(message);
					}
				}
				else if(request.status > 400 && request.status < 405){
					Pebble.showSimpleNotificationOnPebble("Limit Exceeded", "Sorry, this app has reached its daily limit for translation requests.");
				}
			}
		};
		request.open("GET", url, true);
		request.send();
		
	}
	catch(e){
		console.error(e);
	}
}
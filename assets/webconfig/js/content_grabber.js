$(document).ready( function() {
	performTranslation();
	var conf_editor_v4l2 = null;
	var conf_editor_fg = null;
	var conf_editor_instCapt = null;

	// Dynamic v4l2 enum schema
	var v4l2_dynamic_enum_schema = {
		"resolution":
		{
			"enumVals" : 'serverInfo.grabbers.v4l2_properties.resolutions',
			"type": "string",
			"title": "edt_conf_fg_resolution_title",
			"propertyOrder" : 3,
			"required" : true
		},
		"framerate":
		{
			"enumVals" : 'serverInfo.grabbers.v4l2_properties.framerates',
			"type": "string",
			"title": "edt_conf_fg_framerate_title",
			"propertyOrder" : 6,
			"required" : true
    }
  };

	// Build dynamic v4l2 enum schema parts
	var buildSchemaPart = function(key, schema) {
		if (schema[key] && schema[key].enumVals) {
			var enumVals = JSON.parse(JSON.stringify(eval(schema[key].enumVals)));

			window.schema.grabberV4L2.properties[key] = {
				"type": schema[key].type,
				"title": schema[key].title,
				"enum": [].concat(["auto"], enumVals, ["custom"]),
				"options" : {
					"enum_titles" : [].concat(["edt_conf_enum_automatic"], enumVals, ["edt_conf_enum_custom"]),
				},
				"propertyOrder" : schema[key].propertyOrder,
				"required" : schema[key].required
			};
    }
	};

	// Watch all v4l2 dynamic fields
	var setWatchers = function(schema) {
		
		var path = 'root.grabberV4L2.';
		Object.keys(schema).forEach(function(key) {

			conf_editor_v4l2.watch(path + key, function() {
				var ed = conf_editor_v4l2.getEditor(path + key);
				var val = ed.getValue();

				if (ed.key == 'resolution')
				{
					
					if (val != 'custom')
					{
						toggleOption('width', false);
						toggleOption('height', false);
					}
					else
					{
						toggleOption('width', true);
						toggleOption('height', true);
					}

				}

				if (ed.key == 'framerate')
				{
					
					if (val != 'custom')
					{
						toggleOption('fps', false);
					}
					else
					{
						toggleOption('fps', true);
					}

				}

				/////////////////////////////////////////// TODO ///////////////////////////////////////////////////////
				// 1. Unterscheide welcher key ausgewählt ist | if (key == 'resolution') else if (key == 'framerate')
				// 2. Unterscheide ob auto/custom oder etwas anderes ausgewählt ist | if (val == auto || val == custom) else ...
				// 3. Wenn auto/custom NICHT ausgewählt ist:
				//   - breite/höhe oder fps feld ausblenden
				//   - resolution oder framerate feld auslesen und in breite/höhe oder fps feld übertragen
				// 4. Wenn auto/custom ausgewählt ist:
				//   - auto = breite/höhe oder fps feld ausblenden
				//   - custom = breite/höhe oder fps feld einblenden
				////////////////////////////////////////////////////////////////////////////////////////////////////////

			});

		});
	};

	buildSchemaPart('resolution', v4l2_dynamic_enum_schema);
	buildSchemaPart('framerate', v4l2_dynamic_enum_schema);

	function hideEl(el)
	{
		for(var i = 0; i<el.length; i++)
		{
			$('[data-schemapath*="root.framegrabber.'+el[i]+'"]').toggle(false);
		}
	}

	function toggleOption(option, state)
	{
		$('[data-schemapath*="root.grabberV4L2.'+option+'"]').toggle(state);
		$('[data-schemapath*="root.grabberV4L2.'+option+'"]').addClass('col-md-12');
	}

	if(window.showOptHelp)
	{
		// Instance Capture
		$('#conf_cont').append(createRow('conf_cont_instCapt'));
		$('#conf_cont_instCapt').append(createOptPanel('fa-camera', $.i18n("edt_conf_instCapture_heading_title"), 'editor_container_instCapt', 'btn_submit_instCapt'));
		$('#conf_cont_instCapt').append(createHelpTable(window.schema.instCapture.properties, $.i18n("edt_conf_instCapture_heading_title")));

		// Framegrabber
		$('#conf_cont').append(createRow('conf_cont_fg'));
		$('#conf_cont_fg').append(createOptPanel('fa-camera', $.i18n("edt_conf_fg_heading_title"), 'editor_container_fg', 'btn_submit_fg'));
		$('#conf_cont_fg').append(createHelpTable(window.schema.framegrabber.properties, $.i18n("edt_conf_fg_heading_title")));

		// V4L2
		$('#conf_cont').append(createRow('conf_cont_v4l'));
		$('#conf_cont_v4l').append(createOptPanel('fa-camera', $.i18n("edt_conf_v4l2_heading_title"), 'editor_container_v4l2', 'btn_submit_v4l2'));
		$('#conf_cont_v4l').append(createHelpTable(window.schema.grabberV4L2.properties, $.i18n("edt_conf_v4l2_heading_title")));
	}
	else
	{
		$('#conf_cont').addClass('row');
		$('#conf_cont').append(createOptPanel('fa-camera', $.i18n("edt_conf_instCapture_heading_title"), 'editor_container_instCapt', 'btn_submit_instCapt'));
		$('#conf_cont').append(createOptPanel('fa-camera', $.i18n("edt_conf_fg_heading_title"), 'editor_container_fg', 'btn_submit_fg'));
		$('#conf_cont').append(createOptPanel('fa-camera', $.i18n("edt_conf_v4l2_heading_title"), 'editor_container_v4l2', 'btn_submit_v4l2'));
	}
	// Instance Capture
	conf_editor_instCapt = createJsonEditor('editor_container_instCapt', {
		instCapture: window.schema.instCapture
	}, true, true);

	conf_editor_instCapt.on('change',function() {
		conf_editor_instCapt.validate().length ? $('#btn_submit_instCapt').attr('disabled', true) : $('#btn_submit_instCapt').attr('disabled', false);
	});

	$('#btn_submit_instCapt').off().on('click',function() {
		requestWriteConfig(conf_editor_instCapt.getValue());
	});

	// Framegrabber
	conf_editor_fg = createJsonEditor('editor_container_fg', {
		framegrabber: window.schema.framegrabber
	}, true, true);

	conf_editor_fg.on('change',function() {
		conf_editor_fg.validate().length ? $('#btn_submit_fg').attr('disabled', true) : $('#btn_submit_fg').attr('disabled', false);
	});

	$('#btn_submit_fg').off().on('click',function() {
		requestWriteConfig(conf_editor_fg.getValue());
	});

	// V4L2
	conf_editor_v4l2 = createJsonEditor('editor_container_v4l2', {
		grabberV4L2 : window.schema.grabberV4L2
	}, true, true);

	conf_editor_v4l2.on('change',function() {
		conf_editor_v4l2.validate().length ? $('#btn_submit_v4l2').attr('disabled', true) : $('#btn_submit_v4l2').attr('disabled', false);
	});

	conf_editor_v4l2.on('ready', function() {
		setWatchers(v4l2_dynamic_enum_schema);
		
		if (window.serverConfig.grabberV4L2.resolution == 'custom')
		{
			toggleOption('width', true);
			toggleOption('height', true);
		}

		if (window.serverConfig.grabberV4L2.framerate == 'custom')
		{
			toggleOption('fps', true);
		}
		
	});

	$('#btn_submit_v4l2').off().on('click',function() {
		/////////////////////////////////////////// TODO ///////////////////////////////////////////////////////
		// 1. Neue variable erstellen
		// 2. v4l2 config in neue variable übertragen
		// 3. 'v4l2_dynamic_enum_schema' keys aus neuer variable entfernen
		// 4. neue variable abspeichern (requestWriteConfig)
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		var v4l2Options = conf_editor_v4l2.getValue()

		if (v4l2Options.grabberV4L2.resolution != 'custom' && v4l2Options.grabberV4L2.resolution != 'auto')
		{
			v4l2Options.grabberV4L2.width = v4l2Options.grabberV4L2.resolution.split('x')[0]; 
			v4l2Options.grabberV4L2.height = v4l2Options.grabberV4L2.resolution.split('x')[1];
		}

		if (v4l2Options.grabberV4L2.resolution == 'auto')
		{
			v4l2Options.grabberV4L2.width = 0;
			v4l2Options.grabberV4L2.height = 0;
		}

		if (v4l2Options.grabberV4L2.framerate != 'custom' && v4l2Options.grabberV4L2.framerate != 'auto')
			v4l2Options.grabberV4L2.fps = v4l2Options.grabberV4L2.framerate;

		if (v4l2Options.grabberV4L2.framerate == 'auto')
			v4l2Options.grabberV4L2.fps = 0;

		debugger;
		requestWriteConfig(v4l2Options);
	});

	//create introduction
	if(window.showOptHelp)
	{
		createHint("intro", $.i18n('conf_grabber_fg_intro'), "editor_container_fg");
		createHint("intro", $.i18n('conf_grabber_v4l_intro'), "editor_container_v4l2");
	}

	//hide specific options
	conf_editor_fg.on('ready',function() {
		var grabbers = window.serverInfo.grabbers.available;

		if(grabbers.indexOf('dispmanx') > -1)
			hideEl(["device","pixelDecimation"]);
		else if(grabbers.indexOf('x11') > -1)
			hideEl(["device","width","height"]);
		else if(grabbers.indexOf('osx')  > -1 )
			hideEl(["device","pixelDecimation"]);
		else if(grabbers.indexOf('amlogic')  > -1)
			hideEl(["pixelDecimation"]);
	});

	removeOverlay();
});
